/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Infineon CCG8 Power Delivery Controller Driver
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>

#include <drivers/pdc_ucsi.h>

#define DT_DRV_COMPAT realtek_rts54_pdc

#define DEBUG 0

#define RTS54_DATABLOCK_LENGTH 34
typedef uint8_t *rts54_cmd;

#define RTS54_CMD_RETRIES 10
#define RTS54_CMD_DATA_LEN(cmd) cmd[1]
#define RTS54_CMD_LEN(cmd) (RTS54_CMD_DATA_LEN(cmd) + 2)
#define RTS54_CMD_RESULT(ping) (ping & 3)
#define RTS54_CMD_RESULT_LEN(ping) ((ping >> 2) & 0x3F)

#define RTS54_PING_BUSY 0
#define RTS54_PING_COMPLETE 1
#define RTS54_PING_DEFERRED 2
#define RTS54_PING_ERROR 3

#define RTS54_CMD_PPM_RESET           \
	{                             \
		0x0E, 0x02, 0x01, 0x0 \
	}
#define RTS54_CMD_CONNECTOR_RESET          \
	{                                  \
		0x0E, 0x03, 0x03, 0x0, 0x0 \
	}
#define RTS54_CMD_ENABLE_VENDOR              \
	{                                    \
		0x01, 0x03, 0xda, 0x0b, 0x01 \
	}
#define RTS54_CMD_GET_TPC_OP_MODE     \
	{                             \
		0x08, 0x02, 0x9d, 0x0 \
	}
#define RTS54_CMD_SET_TPC_OP_MODE     \
	{                             \
		0x08, 0x03, 0x1d, 0x0 \
	}
#define RTS54_CMD_SET_VOLTAGE         \
	{                             \
		0x1e, 0x03, 0x00, 0x0 \
	}
#define RTS54_CMD_SET_NOTIF_ENABLE                         \
	{                                                  \
		0x08, 0x06, 0x01, 0x00, 0x0, 0x0, 0x0, 0x0 \
	}
#define RTS54_CMD_GET_CAPABILITY       \
	{                              \
		0x0e, 0x02, 0x06, 0x00 \
	}
#define RTS54_CMD_GET_CONNECTOR_CAPABILITY \
	{                                  \
		0x0e, 0x02, 0x07, 0x00     \
	}
#define RTS54_CMD_GET_CONNECTOR_STATUS \
	{                              \
		0x0e, 0x02, 0x12, 0x00 \
	}
#define RTS54_CMD_GET_ERROR_STATUS     \
	{                              \
		0x0e, 0x02, 0x13, 0x00 \
	}
#define RTS54_CMD_SET_UOR                    \
	{                                    \
		0x0e, 0x03, 0x09, 0x00, 0x00 \
	}
#define RTS54_CMD_SET_PDR                    \
	{                                    \
		0x0e, 0x03, 0xb9, 0x00, 0x00 \
	}

enum ppm_event {
	PPM_EVENT_COMPLETE,
	PPM_EVENT_DEFFERED,
	PPM_EVENT_TIMEOUT,
	PPM_EVENT_ERROR,
	PPM_EVENT_COUNT,
};

#define PPM_EVENT_MASK GENMASK(PPM_EVENT_COUNT, 0)

enum ucsi_command_t {
	NO_COMMAND = 0x00, /* not part of the ucsi spec */
	PPM_RESET = 0x01,
	CANCEL = 0x02,
	CONNECTOR_RESET = 0x03,
	ACK_CC_CI = 0x04,
	SET_NOTIFICATION_ENABLE = 0x05,
	GET_CAPABILITY = 0x06,
	GET_CONNECTOR_CAPABILITY = 0x07,
	SET_CCOM = 0x08,
	SET_UOR = 0x09,
	SET_PDM = 0x0A,
	SET_PDR = 0x0B,
	GET_ALTERNATE_MODES = 0x0C,
	GET_CAM_SUPPORTED = 0x0D,
	GET_CURRENT_CAM = 0x0E,
	SET_NEW_CAM = 0x0F,
	GET_PDOS = 0x10,
	GET_CABLE_PROPERTY = 0x11,
	GET_CONNECTOR_STATUS = 0x12,
	GET_ERROR_STATUS = 0x13,
	SET_POWER_LEVEL = 0x14,
	GET_PD_MESSAGE = 0x15,
	GET_ATTENTION_VDO = 0x16,
	GET_CAM_CS = 0x18,
	LPM_FW_UPDATE_REQUEST = 0x19,
	SECURITY_REQUEST = 0x1A,
	SET_RETIMER_MODE = 0x1B,
	SET_SINK_PATH = 0x1C,
};

enum ppm_state_t {
	PPM_INIT,
	PPM_IDLE,
	PPM_ERROR,
	PPM_GET_PING,
	PPM_GET_RESPONSE,
	PPM_CANCEL,
	/* Commands states */
	CMD_VENDOR_CMD_ENABLE,
	/* UCSI Commands */
	CMD_PPM_RESET,
	CMD_CONNECTOR_RESET,
	CMD_SET_NOTIFICATION_ENABLE,
	CMD_GET_CAPABILITY,
	CMD_GET_CONNECTOR_CAPABILITY,
	CMD_SET_CCOM,
	CMD_SET_UOR,
	CMD_SET_PDM,
	CMD_SET_PDR,
	CMD_GET_ALTERNATE_MODES,
	CMD_GET_CAM_SUPPORTED,
	CMD_GET_CURRENT_CAM,
	CMD_SET_NEW_CAM,
	CMD_GET_PDOS,
	CMD_GET_CABLE_PROPERTY,
	CMD_GET_CONNECTOR_STATUS,
	CMD_GET_ERROR_STATUS,
	CMD_SET_POWER_LEVEL,
	CMD_GET_PD_MESSAGE,
	CMD_GET_ATTENTION_VDO,
	CMD_GET_CAM_CS,
	CMD_LPM_FW_UPDATE_REQUEST,
	CMD_SECURITY_REQUEST,
	CMD_SET_RETIMER_MODE,
	CMD_SET_SINK_PATH,
};

static const struct smf_state ppm_states[];

// LOG_MODULE_REGISTER(INTEL_ALTMODE, LOG_LEVEL_ERR);

struct ucsi_data {
	struct smf_ctx ctx;
	const struct device *dev;
	struct k_event evt;
	atomic_t flags;
	union notification_enable_t notification_enable;
	union cci_event_t cci;
	enum ucsi_command_t command;
	uint8_t avail_data;
	uint16_t counter;
	uint16_t version;
	union cc_operation_mode_t ccom;
	union uor_t uor;
	union pdr_t pdr;
	enum port_t port;
	enum port_reset_t reset_type;
	struct pdo_t *pdos;

	bool partner_pdo;
	enum pdo_offset_t pdo_offset;
	uint8_t pdo_num;
	enum power_role_t prole;
	enum source_caps_t source_caps_type;

	bool snk_fet_enable;

	struct connector_status_t *conn_status;
	struct error_status_t *error_status;

	uint8_t *device_caps;
	uint8_t *port_caps[2];

	uint32_t pdo[2];
};

struct pdc_power_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/*
	 * PD interrupt to wake the task to configure alternate modes. There
	 * can be individual Interrupt pin for each PD port or all the PD
	 * interrupts can be muxed to single GPIO. This helps to keep common
	 * code for single port / dual port PD solutions offered by different
	 * PD vendors.
	 */
	struct gpio_dt_spec int_gpio;
};

struct pdc_power_data {
	const struct device *dev;
	struct k_mutex mtx;
	uint8_t write_block[RTS54_DATABLOCK_LENGTH];
	uint8_t read_block[RTS54_DATABLOCK_LENGTH];
	uint8_t rb_length;
	int i2c_rv;
	struct k_work work;
	struct ucsi_data ucsi;
	struct gpio_callback gpio_cb;
	pdc_port_handler_cb_t port_handler_cb;
	pdc_cci_handler_cb_t cci_handler_cb;
};

#if DEBUG
/* List of human readable state names for console debugging */
static const char *const ppm_state_names[] = {
	[PPM_INIT] = "INIT",
	[PPM_IDLE] = "IDLE",
	[PPM_GET_PING] = "GET_PING",
	[PPM_GET_RESPONSE] = "GET_RESPONSE",
	[OPM_CANCEL] = "CANCEL",
	[CMD_PPM_RESET] = "PPM_RESET",
	[CMD_CONNECTOR_RESET] = "CONNECTOR_RESET",
	[CMD_SET_NOTIFICATION_ENABLE] = "SET_NOTIFICATION_ENABLE",
	[CMD_GET_CAPABILITY] = "GET_CAPABILITY",
	[CMD_GET_CONNECTOR_CAPABILITY] = "GET_CONNECTOR_CAPABILITY",

	[CMD_SET_CCOM] = "SET_CCOM",
	[CMD_SET_UOR] = "SET_UOR",
	[CMD_SET_PDM] = "SET_PDM",
	[CMD_SET_PDR] = "SET_PDR",
	[CMD_GET_ALTERNATE_MODES] = "GET_ALTERNATE_MODES",
	[CMD_GET_CAM_SUPPORTED] = "GET_CAM_SUPPORTED",
	[CMD_GET_CURRENT_CAM] = "GET_CURRENT_CAM",
	[CMD_SET_NEW_CAM] = "SET_NEW_CAM",
	[CMD_GET_PDOS] = "GET_PDOS",
	[CMD_GET_CABLE_PROPERTY] = "GET_CABLE_PROPERTY",
	[CMD_GET_CONNECTOR_STATUS] = "GET_CONNECTOR_STATUS",
	[CMD_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
	[CMD_SET_POWER_LEVEL] = "SET_POWER_LEVEL",
	[CMD_GET_PD_MESSAGE] = "GET_PD_MESSAGE",
	[CMD_GET_ATTENTION_VDO] = "GET_ATTENTION_VDO",
	[CMD_GET_CAM_CS] = "GET_CAM_CS",
	[CMD_LPM_FW_UPDATE_REQUEST] = "LPM_FW_UPDATE_REQUEST",
	[CMD_SECURITY_REQUEST] = "SECURITY_REQUEST",
	[CMD_SET_RETIMER_MODE] = "SET_RETIMER_MODE",
	[CMD_SET_SINK_PATH] = "SET_SINK_PATH",
};
#endif

void init_start_handler(struct k_timer *unused);
K_TIMER_DEFINE(init_timer, init_start_handler, NULL);

static void set_ppm_state(struct ucsi_data *ppm,
			  const enum ppm_state_t next_state)
{
	smf_set_state(SMF_CTX(ppm), &ppm_states[next_state]);
}

#if DEBUG
static enum ppm_state_t get_ppm_state(struct ucsi_data *ppm)
{
	return ppm->ctx.current - &ppm_states[0];
}
#endif

static void print_current_ppm_state(struct ucsi_data *ppm)
{
#if DEBUG
	printk("OPM: %s\n", ppm_state_names[get_ppm_state(ppm)]);
#endif
}

static void rts54_send_cmd(const struct device *dev)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *data = dev->data;
	struct i2c_msg msg;

	msg.buf = data->write_block;
	msg.len = RTS54_CMD_LEN(msg.buf);
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	data->i2c_rv = i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int rts54_get_ping(const struct device *dev)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *data = dev->data;
	struct i2c_msg msg;
	uint8_t ping;

	if (data->i2c_rv) {
		/* There was an error on last I2C transaction */
		return -EIO;
	}
	msg.len = 1;
	msg.buf = &ping;
	msg.flags = I2C_MSG_READ | I2C_MSG_STOP;

	data->i2c_rv = i2c_transfer_dt(&cfg->i2c, &msg, 1);
	if (RTS54_CMD_RESULT(ping) == RTS54_PING_BUSY) {
		/* Command is still being processed */
		return -EBUSY;
	} else if (RTS54_CMD_RESULT(ping) == RTS54_PING_DEFERRED) {
		/* Command needs more time, try ping later */
		return -ENOMSG;
	} else if (RTS54_CMD_RESULT(ping) == RTS54_PING_ERROR) {
		/* Command error, try GET_ERROR_STATUS */
		return -EPROTO;
	}

	data->rb_length = RTS54_CMD_RESULT_LEN(ping);
	return 0;
}

static int rts54_get_response(const struct device *dev, uint8_t *resp,
			      uint8_t len)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *data = dev->data;
	struct i2c_msg msg[2];
	uint8_t token = 0x80;
	int rv;

	msg[0].buf = &token;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = data->read_block;
	msg[1].len = data->rb_length + 1;
	msg[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	rv = i2c_transfer_dt(&cfg->i2c, msg, 2);
	if (rv) {
		return -EIO;
	}

	if (len > data->rb_length) {
		len = data->rb_length;
	}
	memcpy(resp, &data->read_block[1], len);

	return 0;
}

void trigger_handler(struct k_work *work)
{
	k_work_submit(work);
}

static void ppm_init_entry(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;

	print_current_ppm_state(ppm);

	ppm->command = NO_COMMAND;
	ppm->counter = 0;
	trigger_handler(&data->work);
}

static void ppm_init_run(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;

	k_mutex_lock(&data->mtx, K_FOREVER);

	set_ppm_state(ppm, CMD_VENDOR_CMD_ENABLE);

	trigger_handler(&data->work);
}

static void ppm_idle_entry(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	print_current_ppm_state(o);

	ppm->command = NO_COMMAND;
}

static void ppm_idle_run(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	switch (ppm->command) {
	case NO_COMMAND:
		break;
	case PPM_RESET:
		set_ppm_state(ppm, CMD_PPM_RESET);
		return;
	case CANCEL:
		break;
	case CONNECTOR_RESET:
		set_ppm_state(ppm, CMD_CONNECTOR_RESET);
		break;
	case ACK_CC_CI:
		break;
	case SET_NOTIFICATION_ENABLE:
		set_ppm_state(ppm, CMD_SET_NOTIFICATION_ENABLE);
		return;
	case GET_CAPABILITY:
		set_ppm_state(ppm, CMD_GET_CAPABILITY);
		break;
	case GET_CONNECTOR_CAPABILITY:
		set_ppm_state(ppm, CMD_GET_CONNECTOR_CAPABILITY);
		break;
	case SET_CCOM:
		set_ppm_state(ppm, CMD_SET_CCOM);
		break;
	case SET_UOR:
		set_ppm_state(ppm, CMD_SET_UOR);
		break;
	case SET_PDM:
		break;
	case SET_PDR:
		set_ppm_state(ppm, CMD_SET_PDR);
		break;
	case GET_ALTERNATE_MODES:
		break;
	case GET_CAM_SUPPORTED:
		break;
	case GET_CURRENT_CAM:
		break;
	case SET_NEW_CAM:
		break;
	case GET_PDOS:
		break;
	case GET_CABLE_PROPERTY:
		break;
	case GET_CONNECTOR_STATUS:
		set_ppm_state(ppm, CMD_GET_CONNECTOR_STATUS);
		break;
	case GET_ERROR_STATUS:
		break;
	case SET_POWER_LEVEL:
		break;
	case GET_PD_MESSAGE:
		break;
	case GET_ATTENTION_VDO:
		break;
	case GET_CAM_CS:
		break;
	case LPM_FW_UPDATE_REQUEST:
		break;
	case SECURITY_REQUEST:
		break;
	case SET_RETIMER_MODE:
		break;
	case SET_SINK_PATH:
		set_ppm_state(ppm, CMD_SET_SINK_PATH);
		break;
	}
}

static void ppm_get_ping_run(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;
	int rv;

	rv = rts54_get_ping(ppm->dev);
	if (rv == 0) {
		set_ppm_state(ppm, PPM_IDLE);
		k_event_post(&ppm->evt, BIT(PPM_EVENT_COMPLETE));
	} else if (rv == -EBUSY) {
		/* Command not completed */
		/* TODO: Handle ping delay */
		trigger_handler(&data->work);
	} else if (rv == -ENOMSG) {
		/* Command deferred */
		/* TODO: Handle deferred message */
		trigger_handler(&data->work);
	} else {
		set_ppm_state(ppm, PPM_IDLE);
		k_event_post(&ppm->evt, BIT(PPM_EVENT_ERROR));
	}
}

static void rts54_create_cmd(const struct device *dev, rts54_cmd cmd)
{
	struct pdc_power_data *data = dev->data;

	memcpy(data->write_block, cmd, RTS54_CMD_LEN(cmd));
}

static int rts54_set_cmd_param(const struct device *dev, uint8_t *param,
			       uint8_t len)
{
	struct pdc_power_data *data = dev->data;

	memcpy(&data->write_block[2], param, len);

	return 0;
}

static void ppm_cancel_entry(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
}

static void cmd_ppm_reset_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_PPM_RESET;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_set_notification_enable_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_SET_NOTIF_ENABLE;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);

	rts54_create_cmd(ppm->dev, cmd);
	rts54_set_cmd_param(ppm->dev,
			    (uint8_t *)&ppm->notification_enable.raw_value, 4);

	rts54_send_cmd(ppm->dev);
}

static void cmd_vendor_cmd_enable_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_ENABLE_VENDOR;
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;

	print_current_ppm_state(ppm);

	k_mutex_lock(&data->mtx, K_FOREVER);

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_vendor_cmd_enable_run(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;
	int rv;

	rv = rts54_get_ping(ppm->dev);
	if (rv == 0) {
		set_ppm_state(ppm, PPM_IDLE);
	} else if (rv < 0) {
		set_ppm_state(ppm, PPM_ERROR);
	}
	trigger_handler(&data->work);
}

static void cmd_vendor_cmd_enable_exit(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;
	struct pdc_power_data *data = (struct pdc_power_data *)ppm->dev->data;

	print_current_ppm_state(ppm);

	k_mutex_unlock(&data->mtx);
}

static void cmd_get_capability_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_GET_CAPABILITY;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);

	cmd[3] = ppm->port;
	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_get_capability_exit(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	rts54_get_response(ppm->dev, ppm->device_caps,
			   sizeof(struct device_capability_t));
}

static void cmd_get_connector_capability_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_GET_CONNECTOR_CAPABILITY;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	cmd[3] = (uint8_t)ppm->port;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_connector_capability_exit(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	rts54_get_response(ppm->dev, ppm->port_caps[ppm->port], 4);
}

static void cmd_connector_reset_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_CONNECTOR_RESET;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	cmd[3] = (uint8_t)ppm->port;
	cmd[4] = (uint8_t)ppm->reset_type;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_set_uor_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_SET_UOR;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	cmd[3] = (uint8_t)ppm->port;
	cmd[4] = ppm->uor.raw_value;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_set_pdr_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_SET_PDR;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	cmd[3] = (uint8_t)ppm->port;
	cmd[4] = ppm->pdr.raw_value;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_get_connector_status_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_CONNECTOR_RESET;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);

	cmd[3] = (uint8_t)ppm->port;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_get_connector_status_exit(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	rts54_get_response(ppm->dev, (uint8_t *)ppm->conn_status,
			   sizeof(struct connector_status_t));
}

static void cmd_get_error_status_entry(void *o)
{
	uint8_t cmd[] = RTS54_CMD_GET_ERROR_STATUS;
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	print_current_ppm_state(ppm);
	cmd[3] = (uint8_t)ppm->port;

	rts54_create_cmd(ppm->dev, cmd);

	rts54_send_cmd(ppm->dev);
}

static void cmd_get_error_status_exit(void *o)
{
	struct ucsi_data *ppm = (struct ucsi_data *)o;

	rts54_get_response(ppm->dev, (uint8_t *)ppm->error_status,
			   sizeof(struct error_status_t));
}

/* Populate state table */
static const struct smf_state ppm_states[] = {
	/* Parent States */
	[PPM_GET_PING] = SMF_CREATE_STATE(NULL, ppm_get_ping_run, NULL, NULL),
	/* Normal States */
	[PPM_INIT] = SMF_CREATE_STATE(ppm_init_entry, ppm_init_run, NULL, NULL),
	[PPM_IDLE] = SMF_CREATE_STATE(ppm_idle_entry, ppm_idle_run, NULL, NULL),
	[CMD_VENDOR_CMD_ENABLE] = SMF_CREATE_STATE(
		cmd_vendor_cmd_enable_entry, cmd_vendor_cmd_enable_run,
		cmd_vendor_cmd_enable_exit, NULL),
	[PPM_CANCEL] = SMF_CREATE_STATE(ppm_cancel_entry, NULL, NULL, NULL),
	[CMD_PPM_RESET] = SMF_CREATE_STATE(cmd_ppm_reset_entry, NULL, NULL,
					   &ppm_states[PPM_GET_PING]),
	[CMD_CONNECTOR_RESET] = SMF_CREATE_STATE(cmd_connector_reset_entry,
						 NULL, NULL,
						 &ppm_states[PPM_GET_PING]),
	[CMD_SET_NOTIFICATION_ENABLE] =
		SMF_CREATE_STATE(cmd_set_notification_enable_entry, NULL, NULL,
				 &ppm_states[PPM_GET_PING]),
	[CMD_GET_CAPABILITY] = SMF_CREATE_STATE(cmd_get_capability_entry, NULL,
						cmd_get_capability_exit,
						&ppm_states[PPM_GET_PING]),
	[CMD_GET_CONNECTOR_CAPABILITY] = SMF_CREATE_STATE(
		cmd_get_connector_capability_entry, NULL,
		cmd_connector_capability_exit, &ppm_states[PPM_GET_PING]),
	[CMD_SET_PDR] = SMF_CREATE_STATE(cmd_set_pdr_entry, NULL, NULL,
					 &ppm_states[PPM_GET_PING]),
	[CMD_SET_UOR] = SMF_CREATE_STATE(cmd_set_uor_entry, NULL, NULL,
					 &ppm_states[PPM_GET_PING]),
	[CMD_GET_ERROR_STATUS] = SMF_CREATE_STATE(
		cmd_get_error_status_entry, NULL, cmd_get_error_status_exit,
		&ppm_states[PPM_GET_PING]),
	[CMD_GET_CONNECTOR_STATUS] = SMF_CREATE_STATE(
		cmd_get_connector_status_entry, NULL,
		cmd_get_connector_status_exit, &ppm_states[PPM_GET_PING]),
};

static int ppm_wait_for_completion(struct ucsi_data *ppm)
{
	int event = k_event_wait(&ppm->evt, PPM_EVENT_MASK, true, Z_FOREVER);

	if (event & BIT(PPM_EVENT_TIMEOUT)) {
		/* Timeout */
		return -ETIME;
	}

	return 0;
}

static void rts54_handler(struct k_work *item)
{
	struct pdc_power_data *data =
		CONTAINER_OF(item, struct pdc_power_data, work);
	struct ucsi_data *ppm = &data->ucsi;

	smf_run_state(SMF_CTX(ppm));
}

/* Driver interface implementation*/
static int rts54_set_handler_cb(const struct device *dev,
				pdc_cci_handler_cb_t cci_cb,
				pdc_port_handler_cb_t port_cb)
{
	struct pdc_power_data *data = dev->data;

	data->cci_handler_cb = cci_cb;
	data->port_handler_cb = port_cb;

	return 0;
}

static int rts54_reset(const struct device *dev)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);
	ppm->command = PPM_RESET;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_cancel(const struct device *dev)
{
	return 0;
}

static int rts54_port_reset(const struct device *dev, enum port_t port,
			    enum port_reset_t type)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->port = port;
	ppm->reset_type = type;
	ppm->command = CONNECTOR_RESET;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_set_notification_enable(const struct device *dev,
					 union notification_enable_t bits)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->notification_enable.raw_value = bits.raw_value;
	ppm->command = SET_NOTIFICATION_ENABLE;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_get_capability(const struct device *dev,
				struct device_capability_t *caps)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->device_caps = (uint8_t *)caps;
	ppm->command = GET_CAPABILITY;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_get_port_capability(const struct device *dev, enum port_t port,
				     union port_capability_t *caps)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->port = port;
	ppm->port_caps[port] = (uint8_t *)caps;
	ppm->command = GET_CONNECTOR_CAPABILITY;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_get_connector_status(const struct device *dev,
				      enum port_t port,
				      struct connector_status_t *cs)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->port = port;
	ppm->conn_status = cs;
	ppm->command = GET_CONNECTOR_STATUS;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static int rts54_get_error_status(const struct device *dev, enum port_t port,
				  struct error_status_t *es)
{
	struct pdc_power_data *data = dev->data;
	struct ucsi_data *ppm = &data->ucsi;
	int ret;

	k_mutex_lock(&data->mtx, K_FOREVER);

	ppm->port = port;
	ppm->error_status = es;
	ppm->command = GET_ERROR_STATUS;
	trigger_handler(&data->work);
	ret = ppm_wait_for_completion(ppm);

	k_mutex_unlock(&data->mtx);

	return ret;
}

static const struct pdc_power_driver_api_t pdc_power_driver_api = {
	.reset = rts54_reset,
	.cancel = rts54_cancel,
	.port_reset = rts54_port_reset,
	.set_notification_enable = rts54_set_notification_enable,
	.get_capability = rts54_get_capability,
	.get_port_capability = rts54_get_port_capability,
	.get_connector_status = rts54_get_connector_status,
	.get_error_status = rts54_get_error_status,
	.set_handler_cb = rts54_set_handler_cb,
};

static void pdc_power_gpio_callback(const struct device *dev,
				    struct gpio_callback *cb, uint32_t pins)
{
	struct pdc_power_data *data =
		CONTAINER_OF(cb, struct pdc_power_data, gpio_cb);

	trigger_handler(&data->work);
}

static int pdc_power_init(const struct device *dev)
{
	const struct pdc_power_config *cfg = dev->config;
	struct pdc_power_data *data = dev->data;
	int rv;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		printk("I2C is not ready\n");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		printk("GPIO is not ready\n");
		return -ENODEV;
	}

	data->ucsi.flags = ATOMIC_INIT(0);
	data->ucsi.dev = dev;
	data->dev = dev;
	k_mutex_init(&data->mtx);
	k_event_init(&data->ucsi.evt);

	printk("\n**PDC DRIVER IS STARTED\n");

	rv = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	if (rv < 0) {
		printk("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pdc_power_gpio_callback,
			   BIT(cfg->int_gpio.pin));

	k_work_init(&data->work, rts54_handler);

	/* TODO: Add GPIO interrupt handlers  */
#if 0
	rv = gpio_add_callback(cfg->int_gpio.port, &data->gpio_cb);
	if (rv < 0) {
		printk("Unable to add callback");
		return rv;
	}
	rv = gpio_pin_interrupt_configure_dt(&cfg->int_gpio, GPIO_INT_EDGE_FALLING);
	if (rv < 0) {
		printk("Unable to configure interrupt");
		return rv;
	}
#endif
	/* Set initial state */
	smf_set_initial(SMF_CTX(&data->ucsi), &ppm_states[PPM_INIT]);
	return 0;
}

#define PDC_POWER_DEFINE(inst)                                                 \
	static struct pdc_power_data pdc_power_data_##inst;                    \
                                                                               \
	static const struct pdc_power_config pdc_power_config##inst = {        \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                             \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),            \
	};                                                                     \
                                                                               \
	DEVICE_DT_INST_DEFINE(inst, pdc_power_init, NULL,                      \
			      &pdc_power_data_##inst, &pdc_power_config##inst, \
			      POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,   \
			      &pdc_power_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_POWER_DEFINE)
