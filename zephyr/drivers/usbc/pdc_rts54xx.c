/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Realtek RTS545x Power Delivery Controller Driver
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/smf.h>
LOG_MODULE_REGISTER(pdc_rts54, LOG_LEVEL_INF);
#include <drivers/pdc.h>

#define DT_DRV_COMPAT realtek_rts54_pdc

#define DEBUG 1

#define BYTE0(n)	((n) & 0xff)
#define BYTE1(n)        (((n) >> 8)  & 0xff)
#define BYTE2(n)        (((n) >> 16) & 0xff)
#define BYTE3(n)        (((n) >> 24) & 0xff)

#define UCSI_VERSION 0x0120
#define T_PING_STATUS 10
#define N_I2C_TRANSACTION_COUNT 10
#define N_RETRY_COUNT	200

#define MAX_DATA_BUFFER_SIZE 256

enum cmd_sts_t {
	/* Command has not been started */
	CMD_BUSY	= 0,
	/* Command has completed */
	CMD_DONE	= 1,
	/* Command has been started but has not completed */
	CMD_DEFERRED	= 2,
	/* Command completed with error. Send GET_ERROR_STATUS for details */
	CMD_ERROR	= 3
};

union ping_status_t {
	struct {
		uint8_t cmd_sts  : 2;
		uint8_t data_len : 6;
	};
	uint8_t raw_value;
};

enum state_t {
	ST_IDLE,
	ST_WRITE,
	ST_PING_STATUS,
	ST_READ
};

enum cmd_t {
	CMD_NONE,
	CMD_VENDOR_ENABLE,
	CMD_SET_NOTIFICATION_ENABLE,
	CMD_PPM_RESET,
	CMD_CONNECTOR_RESET,
	CMD_GET_CAPABILITY,
	CMD_GET_CONNECTOR_CAPABILITY,
	CMD_SET_UOR,
	CMD_SET_PDR,
	CMD_GET_ALTERNATE_MODE,
	CMD_GET_PDO,
	CMD_GET_CONNECTOR_STATUS,
	CMD_GET_ERROR_STATUS,
	CMD_GET_VBUS_VOLTAGE,
	CMD_GET_VBUS_CURRENT,


	CMD_IS_PD_READY,
	CMD_IS_TYPEC_CONNECTED,
	CMD_IS_FLASH_CODE,
	CMD_GET_FW_VERSION,
	CMD_GET_VIDPID,
	CMD_GET_PD_VERSION,
	CMD_SET_CCOM,

	CMD_READ_POWER_LEVEL,
	CMD_GET_RDO,

	CMD_SET_SINK_PATH,
	CMD_GET_CURRENT_PARTNER_SRC_PDO,

	CMD_SEND_PD_COMMAND,
};

struct pdc_config_t {
	/* I2C config */
	struct i2c_dt_spec i2c;
	 /* pdc power path interrupt */
	struct gpio_dt_spec irq_gpios;
	/* connector number of this port */
	uint8_t connector_number;

	void (*create_thread)(const struct device *dev);
};

struct pdc_data_t {
	struct smf_ctx ctx;
	const struct device *dev;
	enum cmd_t cmd;

	/** This port's thread */
	k_tid_t thread;
	/** This port thread's data */
	struct k_thread thread_data;

	union ping_status_t ping_status;

	uint8_t ping_retry_counter;
	uint8_t i2c_transaction_retry_counter;

	uint8_t wr_buf[MAX_DATA_BUFFER_SIZE];
	uint8_t wr_buf_len;

	uint8_t rd_buf[MAX_DATA_BUFFER_SIZE];
	uint8_t rd_buf_len;

	uint8_t *user_buf;

	struct k_mutex mtx;
	struct k_work work;
	struct gpio_callback gpio_cb;

	union error_status_t error_status;
	union cci_event_t cci_event;
	pdc_cci_handler_cb_t cci_cb;
};

static const char *const cmd_names[] = {
	[CMD_NONE] = "",
	[CMD_VENDOR_ENABLE] = "VENDOR_ENABLE",
	[CMD_SET_NOTIFICATION_ENABLE] = "SET_NOTIFICATION_ENABLE",
	[CMD_PPM_RESET] = "PPM_RESET",
	[CMD_CONNECTOR_RESET] = "CONNECTOR_RESET",
	[CMD_GET_CAPABILITY] = "GET_CAPABILITY",
	[CMD_GET_CONNECTOR_CAPABILITY] = "GET_CONNECTOR_CAPABILITY",
	[CMD_SET_UOR] = "SET_UOR",
	[CMD_SET_PDR] = "SET_PDR",
	[CMD_GET_ALTERNATE_MODE] = "GET_ALTERNATE_MODE",
	[CMD_GET_PDO] = "GET_PDO",
	[CMD_GET_CONNECTOR_STATUS] = "GET_CONNECTOR_STATUS",
	[CMD_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
	[CMD_GET_VBUS_VOLTAGE] = "GET_VBUS_VOLTAGE",
	[CMD_GET_VBUS_CURRENT] = "GET_VBUS_CURRENT",
	[CMD_IS_PD_READY] = "IS_PD_READY",
	[CMD_IS_TYPEC_CONNECTED] = "IS_TYPEC_CONNECTED",
	[CMD_IS_FLASH_CODE] = "IS_FLASH_CODE",
	[CMD_GET_FW_VERSION] = "GET_FW_VERSION",
	[CMD_GET_VIDPID] = "GET_VIDPID",
	[CMD_GET_PD_VERSION] = "GET_PD_VERSION",
	[CMD_SET_CCOM] = "SET_CCOM",
	[CMD_SET_SINK_PATH] = "SET_SINK_PATH",
	[CMD_READ_POWER_LEVEL] = "READ_POWER_LEVEL",
	[CMD_GET_RDO] = "GET_RDO",
};

/* List of human readable state names for console debugging */
static const char *const state_names[] = {
	[ST_IDLE] = "IDLE",
	[ST_WRITE] = "WRITE",
	[ST_PING_STATUS] = "PING_STATUS",
	[ST_READ] = "READ",
};

static const struct smf_state states[];

static void set_state(struct pdc_data_t *data, const enum state_t next_state)
{
	smf_set_state(SMF_CTX(data), &states[next_state]);
}

static enum state_t get_state(struct pdc_data_t *data)
{
	return data->ctx.current - &states[0];
}

static void print_current_state(struct pdc_data_t *data)
{
	int st = get_state(data);

	if (st == ST_WRITE) {
		LOG_INF("ST: %s %s", state_names[st], cmd_names[data->cmd]);
	} else {
		LOG_INF("ST: %s", state_names[get_state(data)]);
	}
}

static void call_cci_event_cb(struct pdc_data_t *data)
{
	if (data->cci_cb) {
		data->cci_cb(data->cci_event);
	}
}

static int get_ara(const struct device *dev, uint8_t *ara)
{
	const  struct pdc_config_t *cfg = dev->config;

	return i2c_read(cfg->i2c.bus, ara, 1, 0x0C);
}

static int get_ping_status(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	const  struct pdc_config_t *cfg = dev->config;
	struct i2c_msg msg;

	msg.buf = &data->ping_status.raw_value;
	msg.len = 1;
	msg.flags = I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static int rts54_i2c_read(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	const  struct pdc_config_t *cfg = dev->config;
	struct i2c_msg msg[2];
	uint8_t cmd = 0x80;
	int rv;

	msg[0].buf = &cmd;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = data->rd_buf;
	msg[1].len = data->ping_status.data_len + 1;
	msg[1].flags = I2C_MSG_READ | I2C_MSG_STOP;

	rv = i2c_transfer_dt(&cfg->i2c, msg, 2);
	if (rv < 0) {
		return rv;
	}

	data->rd_buf_len = data->ping_status.data_len;

	return rv;
}

static int rts54_i2c_write(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	const  struct pdc_config_t *cfg = dev->config;
	struct i2c_msg msg;

#if DEBUG
	printk("WR: ");
	for (int i = 0; i < data->wr_buf_len; i++) {
		printk("%02x ", data->wr_buf[i]);
	}
	printk("\n");
#endif

	msg.buf = data->wr_buf;
	msg.len = data->wr_buf_len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static void st_idle_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);

	data->cmd = CMD_NONE;
}

static void st_idle_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	if (data->cmd != CMD_NONE) {
		set_state(data, ST_WRITE);
	}
}

static void st_write_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	const  struct pdc_config_t *cfg = data->dev->config;

	print_current_state(data);
	data->i2c_transaction_retry_counter = 0;

	/* Clear the Error Status */
	data->error_status.raw_value = 0;
	/* Clear the CCI Event */
	data->cci_event.raw_value = 0;
	/* Set the port the CCI Event occurred on */
	data->cci_event.connector_change = cfg->connector_number;
}

static void st_write_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	int rv;

	/* Write the command */
	rv = rts54_i2c_write(data->dev);
	if (rv < 0) {
		/* I2C write failed */
		data->i2c_transaction_retry_counter++;
		if (data->i2c_transaction_retry_counter > N_I2C_TRANSACTION_COUNT) {
			/* MAX I2C transactions exceeded */

			/*
			 * The command was not successfully completed,
			 * so set cci.error to 1b.
			 */
			data->cci_event.error = 1;
			data->cci_event.command_completed = 1;
			data->error_status.i2c_write_error = 1;
			/* Notify system of status change */
			call_cci_event_cb(data);

			/* All done, return to idle state */
			set_state(data, ST_IDLE);
		}
		return;
	}

	/* I2C transaction succeeded */
	set_state(data, ST_PING_STATUS);
}

static void st_ping_status_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	const  struct pdc_config_t *cfg = data->dev->config;

	print_current_state(data);
	data->i2c_transaction_retry_counter = 0;
	data->ping_retry_counter = 0;
	data->ping_status.raw_value = 0;

	/* Clear the Error Status */
	data->error_status.raw_value = 0;
	/* Clear the CCI Event */
	data->cci_event.raw_value = 0;
	/* Set the port the CCI Event occurred on */
	data->cci_event.connector_change = cfg->connector_number;
}

static void st_ping_status_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	int rv;

	rv = get_ping_status(data->dev);
	if (rv < 0) {
		/* I2C transaction failed */
		data->i2c_transaction_retry_counter++;
		if (data->i2c_transaction_retry_counter > N_I2C_TRANSACTION_COUNT) {
			/* MAX I2C transactions exceeded */
			/*
			 * The command was not successfully completed,
			 * so set cci.error to 1b.
			 */
			data->cci_event.error = 1;
			data->cci_event.command_completed = 1;
			data->error_status.i2c_read_error = 1;
			/* Notify system of status change */
			call_cci_event_cb(data);

			/* All done, return to idle state */
			set_state(data, ST_IDLE);
		}
		return;
	}

	switch (data->ping_status.cmd_sts) {
	case CMD_BUSY:
		/*
		 * Busy and Defered and handled the same,
		 * so fall through
		 */
	case CMD_DEFERRED:
		/*
		 * The command has not been processed.
		 * Stay here and resend get ping status.
		 */
		data->ping_retry_counter++;
		if (data->ping_retry_counter > N_RETRY_COUNT) {
			/* MAX Ping Retries exceeded */
			/*
			 * The command was not successfully completed,
			 * so set cci.error to 1b.
			 */
			data->cci_event.error = 1;
			data->cci_event.command_completed = 1;
			data->error_status.ping_retry_count = 1;

			/* Notify system of status change */
			call_cci_event_cb(data);
			set_state(data, ST_IDLE);
		} else {
			/*
			 * If Busy, then set this cci.busy to a 1b
			 * and all other fields to zero.
			 */
			data->cci_event.busy = 1;

			/* Notify system of status change */
			call_cci_event_cb(data);
		}
		break;
	case CMD_DONE:
		if (data->cmd == CMD_PPM_RESET) {
			/* The PDC has been reset,
			 * so set cci.reset_completed to 1b.
			 */
			data->cci_event.reset_completed = 1;
			/* Notify system of status change */
			call_cci_event_cb(data);

			/* All done, return to idle state */
			set_state(data, ST_IDLE);
		} else {
#if DEBUG
			LOG_INF("ping_status: %02x", data->ping_status.raw_value);
#endif
			/*
			 * The command completed successfully,
			 * so set cci.command_completed to 1b.
			 */
			data->cci_event.command_completed = 1;

			if (data->ping_status.data_len > 0) {
				/* Data is available, so read it */
				set_state(data, ST_READ);
			} else {
				/* Inform the system of the event */
				call_cci_event_cb(data);

				/* All done, return to idle state */
				set_state(data, ST_IDLE);
			}
		}
		break;
	case CMD_ERROR:
		/*
		 * The command was not successfully completed,
		 * so set cci.error to 1b.
		 */
		data->cci_event.error = 1;
		data->cci_event.command_completed = 1;

		/* Notify system of status change */
		call_cci_event_cb(data);

		/* All done, return to idle state */
		set_state(data, ST_IDLE);
		break;
	}
}

static void st_read_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	const  struct pdc_config_t *cfg = data->dev->config;

	print_current_state(data);

	/* Clear the Error Status */
	data->error_status.raw_value = 0;
	/* Clear the CCI Event */
	data->cci_event.raw_value = 0;
	/* Set the port the CCI Event occurred on */
	data->cci_event.connector_change = cfg->connector_number;
}

static void st_read_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	uint8_t offset;
	uint8_t len;
	int rv;

	rv = rts54_i2c_read(data->dev);
	if (rv < 0) {
		/*
		 * The command was not successfully completed,
		 * so set cci.error to 1b.
		 */
		data->cci_event.error = 1;
		data->cci_event.command_completed = 1;
		data->error_status.i2c_read_error = 1;

		/* Notify system of status change */
		call_cci_event_cb(data);

		/* All done, return to idle state */
		set_state(data, ST_IDLE);
		return;
	}

	/* Get length of data returned */
	len = data->rd_buf[0];

	/* Skip over length byte */
	offset = 1;
#if DEBUG
	/* Print read data */
	printk("RD: ");
	for (int i = 0; i < len+1; i++) {
		printk("%02x ", data->rd_buf[i]);
	}
	printk("\n");
#endif

	/* Copy the received data to the user's buffer */
	switch (data->cmd) {
	case CMD_IS_PD_READY:
		*data->user_buf = data->rd_buf[1] & 0x01;
		break;
	case CMD_IS_TYPEC_CONNECTED:
		*data->user_buf = !!(data->rd_buf[1] & 0x08);
		break;
	case CMD_GET_FW_VERSION:
		*(uint32_t *)data->user_buf = data->rd_buf[1] << 16 |
					      data->rd_buf[2] << 8 |
					      data->rd_buf[3];
		break;
	case CMD_GET_VIDPID:
		*(uint32_t *)data->user_buf = data->rd_buf[1] << 24 |
					      data->rd_buf[2] << 16 |
					      data->rd_buf[3] << 8 |
					      data->rd_buf[4];
		break;
	case CMD_GET_PD_VERSION:
		*(uint32_t *)data->user_buf = data->rd_buf[1] << 24 |
					      data->rd_buf[2] << 16 |
					      data->rd_buf[3] << 8 |
					      data->rd_buf[4];
		break;
	case CMD_GET_CONNECTOR_STATUS:
		/* Map Realtek GET_RTK_STATUS bits to UCSI GET_CONNECTOR_STATUS */
		struct connector_status_t *cs = (struct connector_status_t *)data->user_buf;

		/*
		 * NOTE: Realtek sets an additional 16-bits of status_change
		 *       events in bytes 3 and 4, but they are not part of the
		 *       UCSI spec, so are ignored.
		 */
		cs->conn_status_change_bits.raw_value = data->rd_buf[1] << 16 |
						   data->rd_buf[2];
						   /* ignore data->rd_buf[3] */
						   /* ignore data->rd_buf[4] */

		/* Realtek Port Operation Mode: Byte5, Bit1:3 */
		cs->power_operation_mode = ((data->rd_buf[5] >> 1) & 7);

		/* Realtek Connection Status: Byte5, Bit7 */
		cs->connect_status = ((data->rd_buf[5] >> 7) & 1);

		/* Realtek Power Direction: Byte5, Bit6 */
		cs->power_direction = ((data->rd_buf[5] >> 6) & 1);

		/* Realtek Connector Partner Flags: Byte6, Bit0:7 */
		cs->conn_partner_flags = data->rd_buf[6];

		/* Realtek Connector Partner Type: Byte11, Bit0:2 */
		cs->conn_partner_flags = (data->rd_buf[11] & 7);

		/* Realtek RDO: Bytes [7:10] */
		cs->rdo = data->rd_buf[10] << 24 |
			  data->rd_buf[9] << 16 |
			  data->rd_buf[8] << 8 |
			  data->rd_buf[7];

		/* Realtek Battery Charging Capability Status, Byte 11, Bit3:4 */
		cs->battery_charging_cap = 0; /* NOTE: Not set in this register by Realtek */
		cs->provider_caps_limited = 0; /* NOTE: Not set in this register by Realtek */

		/* Realtek bcdPDVersion Operation Mode, Byte13, Bit6:7 */
		cs->bcd_pd_version = ((((data->rd_buf[13] >> 6) & 3) + 1) << 8);

		/* Realtek Plug Direction, Byte 12, Bit5 */
		cs->orientation = ((data->rd_buf[12] >> 5) & 1);

		/* Realtek VBSIN_EN switch status, Byte 13, Bit0:1 */
		cs->sink_path_status = (((data->rd_buf[13]) & 3) == 3);

		/* Realtek NOT SET */
		cs->reverse_current_protection_status = 0;
		cs->power_reading_ready = 0;
		cs->current_scale = 0;
		cs->peak_current = 0;
		cs->average_current = 0;

		/* Realtek voltage scale is 1010b - 50mV */
		cs->voltage_scale = 0xa;

		/* Realtek Voltage Reading Byte 17 (low byte) and Byte 18 (high byte) */
		cs->voltage_reading = data->rd_buf[18] << 8 |
				      data->rd_buf[17];
		break;
	case CMD_GET_ERROR_STATUS:
		/* Map Realtek GET_ERROR_STATUS bits to UCSI GET_ERROR_STATUS */
		union error_status_t *es = (union error_status_t *)data->user_buf;

		es->unrecognized_command = (data->rd_buf[1] & 1);
		es->non_existent_connector_number = ((data->rd_buf[1] >> 1) & 1);
		es->invalid_command_specific_param = ((data->rd_buf[1] >> 2) & 1);
		es->incompatible_connector_partner = ((data->rd_buf[1] >> 3) & 1);
		es->cc_communication_error = ((data->rd_buf[1] >> 4) & 1);
		es->cmd_unsuccessful_dead_batt = ((data->rd_buf[1] >> 5) & 1);
		es->contract_negotiation_failed = ((data->rd_buf[1] >> 6) & 1);
		es->overcurrent = ((data->rd_buf[1] >> 7) & 1);

		/* NOTE: Vendor Specific Error were already set in previous states */
		break;
	default:
		/* No preprocessing needed for the user data */
		memcpy(data->user_buf, data->rd_buf + offset, len);
	}

	/* Clear the read buffer */
	memset(data->rd_buf, 0, 256);

	/*
	 * Set cci.data_len. This will be zero if no
	 * data is available.
	 */
	data->cci_event.data_len = len;
	data->cci_event.command_completed = 1;

	/* Inform the system of the event */
	call_cci_event_cb(data);

	/* All done, return to idle state */
	set_state(data, ST_IDLE);
}

/* Populate cmd state table */
static const struct smf_state states[] = {
	[ST_IDLE] = SMF_CREATE_STATE(
		st_idle_entry,
		st_idle_run,
		NULL,
		NULL),
	[ST_WRITE] = SMF_CREATE_STATE(
		st_write_entry,
		st_write_run,
		NULL,
		NULL),
	[ST_PING_STATUS] = SMF_CREATE_STATE(
		st_ping_status_entry,
		st_ping_status_run,
		NULL,
		NULL),
	[ST_READ] = SMF_CREATE_STATE(
		st_read_entry,
		st_read_run,
		NULL,
		NULL),
};

static void trigger_handler(struct k_work *work)
{
	k_work_submit(work);
}

static int rts54_get_ic_status(const struct device *dev, uint8_t offset, uint8_t len, enum cmd_t cmd, uint8_t *buf)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if ((offset > 0x1e) || (buf == NULL)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x3A;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = offset;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = len;
	data->wr_buf_len = 5;
	data->user_buf = buf;
	data->cmd = cmd;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_rtk_status(const struct device *dev, uint8_t offset, uint8_t len, enum cmd_t cmd, uint8_t *buf)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (buf == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x09;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = offset;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = len;
	data->wr_buf_len = 5;
	data->user_buf = buf;
	data->cmd = cmd;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_ucsi_version(const struct device *dev, uint16_t *version)
{
	if (version == NULL) {
		return -EINVAL;
	}

	*version = UCSI_VERSION;

	return 0;
}

static int rts54_set_handler_cb(const struct device *dev, pdc_cci_handler_cb_t cci_cb)
{
	struct pdc_data_t *data = dev->data;

	data->cci_cb = cci_cb;

	return 0;
}

static int rts54_enable(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x01;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0xDA;
	data->wr_buf[3] = 0x0B;
	data->wr_buf[4] = 0x01;
	data->wr_buf_len = 5;
	data->cmd = CMD_VENDOR_ENABLE;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_read_power_level(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x1E;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = 0x00;
	data->wr_buf_len = 5;
	data->cmd = CMD_READ_POWER_LEVEL;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_reset(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x01;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->cmd = CMD_PPM_RESET;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_connector_reset(const struct device *dev, enum connector_reset_t type)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x03;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = type;
	data->wr_buf_len = 5;
	data->cmd = CMD_CONNECTOR_RESET;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_set_sink_path(const struct device *dev, bool en)
{
	struct pdc_data_t *data = dev->data;
	uint8_t byte;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	if (en) {
		byte = 0x8c;
	} else {
		byte = 0x80;
	}

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x21;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = byte;
	data->wr_buf_len = 5;
	data->cmd = CMD_SET_SINK_PATH;

	k_mutex_unlock(&data->mtx);


	return 0;
}

static int rts54_set_notification_enable(const struct device *dev, union notification_enable_t bits, uint16_t ext_bits)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x06;
	data->wr_buf[2] = 0x01;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = BYTE0(bits.raw_value);
	data->wr_buf[5] = BYTE1(bits.raw_value);
	data->wr_buf[6] = BYTE0(ext_bits);
	data->wr_buf[7] = BYTE1(ext_bits);
	data->wr_buf_len = 8;
	data->cmd = CMD_SET_NOTIFICATION_ENABLE;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_capability(const struct device *dev, struct capability_t *caps)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (caps == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x06;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)caps;
	data->cmd = CMD_GET_CAPABILITY;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_connector_capability(const struct device *dev, union connector_capability_t *caps)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (caps == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x07;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)caps;
	data->cmd = CMD_GET_CONNECTOR_CAPABILITY;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_connector_status(const struct device *dev, struct connector_status_t *cs)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (cs == NULL) {
		return -EINVAL;
	}
#if 0
	/*
	 * NOTE: Realtek's get connector status command doesn't provide all the
	 * information in the UCSI get connector status command, but the
	 * get rtk status command comes close.
	 */

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x12;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)cs;
	data->cmd = CMD_GET_CONNECTOR_STATUS;

	k_mutex_unlock(&data->mtx);
#endif

	return rts54_get_rtk_status(dev, 0, 18, CMD_GET_CONNECTOR_STATUS, (uint8_t *)cs);
}

static int rts54_get_error_status(const struct device *dev, union error_status_t *es)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (es == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x13;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)es;
	data->cmd = CMD_GET_ERROR_STATUS;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_rdo(const struct device *dev, uint32_t *rdo)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (rdo == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x84;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)rdo;
	data->cmd = CMD_GET_RDO;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_pdos(const struct device *dev, enum pdo_type_t pdo_type,
			enum pdo_offset_t pdo_offset, uint8_t num_pdos,
			bool port_partner_pdo, uint32_t *pdos)
{
	struct pdc_data_t *data = dev->data;
	uint8_t byte4;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (pdos == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	byte4 = (num_pdos << 5) | (pdo_offset << 2) | (port_partner_pdo << 1) | pdo_type;

	memset((uint8_t *)pdos, 0, 4 * num_pdos);

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x83;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = byte4;
	data->wr_buf_len = 5;
	data->user_buf = (uint8_t *)pdos;
	data->cmd = CMD_GET_PDO;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_is_flash_code(const struct device *dev, uint8_t *is_flash_code)
{
	if (is_flash_code == NULL) {
		return -EINVAL;
	}

	return rts54_get_ic_status(dev, 0, 1, CMD_IS_FLASH_CODE, is_flash_code);
}

static int rts54_get_fw_version(const struct device *dev, uint32_t *fw_version)
{
	if (fw_version == NULL) {
		return -EINVAL;
	}

	*fw_version = 0;
	return rts54_get_ic_status(dev, 3, 3, CMD_GET_FW_VERSION, (uint8_t *)fw_version);
}

static int rts54_get_vid_pid(const struct device *dev, uint32_t *vidpid)
{
	if (vidpid == NULL) {
		return -EINVAL;
	}

	return rts54_get_ic_status(dev, 9, 4, CMD_GET_VIDPID, (uint8_t *)vidpid);
}

static int rts54_get_pd_version(const struct device *dev, uint32_t *pd_version)
{
	if (pd_version == NULL) {
		return -EINVAL;
	}

	return rts54_get_ic_status(dev, 22, 4, CMD_GET_PD_VERSION, (uint8_t *)pd_version);
}

static int rts54_get_vbus_voltage(const struct device *dev, uint16_t *voltage)
{
	if (voltage == NULL) {
		return -EINVAL;
	}

	return rts54_get_rtk_status(dev, 16, 2, CMD_GET_VBUS_VOLTAGE, (uint8_t *)voltage);
}

static int rts54_set_ccom(const struct device *dev, enum ccom_t ccom, enum drp_mode_t dm)
{
	struct pdc_data_t *data = dev->data;
	uint8_t byte = 0;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	switch (ccom) {
	case CCOM_RP:
		byte = 0x02;
		break;
	case CCOM_DRP:
		byte = 0x01;
		switch (dm) {
		case DRP_NORMAL:
			/* No Try.Src or Try.Snk */
			break;
		case DRP_TRY_SRC:
			byte |= (1 << 3);
			break;
		case DRP_TRY_SNK:
			byte |= (2 << 3);
			break;
		}
		break;
	case CCOM_RD:
		byte = 0;
		break;
	}

	/* We always want Accessory Support */
	byte |= (1 << 2);

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x1D;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = byte;
	data->wr_buf_len = 5;
	data->cmd = CMD_SET_CCOM;

	k_mutex_unlock(&data->mtx);


	return 0;
}

static int rts54_set_uor(const struct device *dev, union uor_t uor)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x09;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = uor.raw_value;
	data->wr_buf_len = 5;
	data->cmd = CMD_SET_UOR;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_set_pdr(const struct device *dev, union pdr_t pdr)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x03;
	data->wr_buf[2] = 0x0B;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = pdr.raw_value;
	data->wr_buf_len = 5;
	data->cmd = CMD_SET_PDR;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_current_pdo(const struct device *dev, uint32_t *pdo)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (pdo == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x08;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0xA7;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)pdo;
	data->cmd = CMD_GET_CURRENT_PARTNER_SRC_PDO;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static const struct pdc_driver_api_t pdc_driver_api = {
	.enable = rts54_enable,
	.get_ucsi_version = rts54_get_ucsi_version,
	.reset = rts54_reset,
	.connector_reset = rts54_connector_reset,
	.set_notification_enable = rts54_set_notification_enable,
	.get_capability = rts54_get_capability,
	.get_connector_capability = rts54_get_connector_capability,
	.set_ccom = rts54_set_ccom,
	.set_uor = rts54_set_uor,
	.set_pdr = rts54_set_pdr,
	.set_sink_path = rts54_set_sink_path,
	.get_connector_status = rts54_get_connector_status,
	.get_pdos = rts54_get_pdos,
	.get_rdo = rts54_get_rdo,
	.get_error_status = rts54_get_error_status,
	.get_vbus_voltage = rts54_get_vbus_voltage,
	.get_current_pdo = rts54_get_current_pdo,
	.set_handler_cb = rts54_set_handler_cb,

	.read_power_level = rts54_read_power_level,

	.is_flash_code = rts54_is_flash_code,
	.get_fw_version = rts54_get_fw_version,
	.get_vid_pid = rts54_get_vid_pid,
	.get_pd_version = rts54_get_pd_version,
};

static void interrupt_handler(struct k_work *item)
{
	struct pdc_data_t *data = CONTAINER_OF(item,  struct pdc_data_t, work);
	uint8_t ara;

	get_ara(data->dev, &ara);

	LOG_INF("IRQ: %02x\n", ara);

	/* INTERRUPTS DON'T SEEM TO BE WORKING */

	/* TODO: add functionality */
}

static void pdc_interrupt_callback(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	struct pdc_data_t *data =
		CONTAINER_OF(cb,  struct pdc_data_t, gpio_cb);

	trigger_handler(&data->work);
}

static int pdc_init(const struct device *dev)
{
	const  struct pdc_config_t *cfg = dev->config;
	struct pdc_data_t *data = dev->data;
	int rv;

	rv = i2c_is_ready_dt(&cfg->i2c);
	if (rv < 0) {
		LOG_ERR("device %s not ready", cfg->i2c.bus->name);
		return -ENODEV;
	}

	rv = gpio_is_ready_dt(&cfg->irq_gpios);
	if (rv < 0) {
		LOG_ERR("device %s not ready", cfg->irq_gpios.port->name);
		return -ENODEV;
	}

	rv = gpio_pin_configure_dt(&cfg->irq_gpios, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pdc_interrupt_callback, BIT(cfg->irq_gpios.pin));

	rv = gpio_add_callback(cfg->irq_gpios.port, &data->gpio_cb);
	if (rv < 0) {
		LOG_ERR("Unable to add callback");
		return rv;
	}

	rv = gpio_pin_interrupt_configure_dt(&cfg->irq_gpios, GPIO_INT_EDGE_FALLING);
	if (rv < 0) {
		LOG_ERR("Unable to configure interrupt");
		return rv;
	}

	k_mutex_init(&data->mtx);
	k_work_init(&data->work, interrupt_handler);

	data->dev = dev;
	data->cmd = CMD_NONE;

	/* Set initial state */
	smf_set_initial(SMF_CTX(data), &states[ST_IDLE]);

	/* Create the thread for this port */
	cfg->create_thread(dev);

	LOG_INF("Realtek RTS545x PDC DRIVER");

	return 0;
}

#define PDC_DEFINE(inst)								\
	K_THREAD_STACK_DEFINE(thread_stack_area_##inst, CONFIG_USBC_PDC_STACK_SIZE);	\
											\
	static void run_driver_##inst(void *dev, void *unused1, void *unused2)		\
	{										\
		struct pdc_data_t *data = ((const struct device *)dev)->data;		\
											\
		while (1) {								\
			smf_run_state(SMF_CTX(data));					\
			k_sleep(K_MSEC(T_PING_STATUS));					\
		}									\
	}										\
											\
	static void create_thread_##inst(const struct device *dev)			\
	{										\
		struct pdc_data_t *data = dev->data;					\
											\
		data->thread = k_thread_create(						\
			&data->thread_data, thread_stack_area_##inst,			\
			K_THREAD_STACK_SIZEOF(thread_stack_area_##inst),		\
			run_driver_##inst, (void *)dev,					\
			0, 0, CONFIG_USBC_PDC_THREAD_PRIORITY,				\
			K_ESSENTIAL, K_NO_WAIT);					\
	}										\
											\
	static  struct pdc_data_t pdc_data_##inst;					\
											\
	static const  struct pdc_config_t pdc_config##inst = {				\
		.i2c = I2C_DT_SPEC_INST_GET(inst),					\
		.irq_gpios = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),			\
		.connector_number = 0, /* TODO: Read from DT */				\
		.create_thread = create_thread_##inst,					\
	};										\
											\
	DEVICE_DT_INST_DEFINE(inst, pdc_init, NULL,					\
			      &pdc_data_##inst, &pdc_config##inst,			\
			      POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY,		\
			      &pdc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_DEFINE)
