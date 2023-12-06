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
	ST_WAIT,
	ST_READ,
	ST_ERROR,
	ST_GET_ERROR_STATUS
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
	CMD_GET_IC_STATUS,

	CMD_SET_CCOM,
#if 0
	CMD_SET_PDM,
	CMD_GET_CAM_SUPPORTED,
	CMD_GET_CURRENT_CAM,
	CMD_SET_NEW_CAM,
	CMD_GET_CABLE_PROPERTY,
	CMD_SET_POWER_LEVEL,
	CMD_GET_PD_MESSAGE,
	CMD_GET_ATTENTION_VDO,
	CMD_GET_CAM_CS,
	CMD_LPM_FW_UPDATE_REQUEST,
	CMD_SECURITY_REQUEST,
	CMD_SET_RETIMER_MODE,
#endif
	CMD_SET_SINK_PATH,
};

struct pdc_config_t {
	/* I2C config */
	struct i2c_dt_spec i2c;
	 /* pdc power path interrupt */
	struct gpio_dt_spec irq_gpios;

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

	uint8_t wr_buf[256];
	uint8_t wr_buf_len;

	uint8_t rd_buf[256];
	uint8_t rd_buf_len;

	uint8_t *user_buf;

	struct k_mutex mtx;
	struct k_work work;
	struct gpio_callback gpio_cb;
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
	[CMD_GET_IC_STATUS] = "GET_IC_STATUS",
	[CMD_SET_CCOM] = "SET_CCOM",
	[CMD_SET_SINK_PATH] = "SET_SINK_PATH",
};

/* List of human readable state names for console debugging */
static const char *const state_names[] = {
	[ST_IDLE] = "IDLE",
	[ST_WRITE] = "WRITE",
	[ST_WAIT] = "WAIT",
	[ST_READ] = "READ",
	[ST_ERROR] = "ERROR",
	[ST_GET_ERROR_STATUS] = "GET_ERROR_STATUS",
};

static void create_thread(const struct device *dev);
static const struct smf_state states[];

K_THREAD_STACK_DEFINE(thread_stack_area, 2000);

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

static int get_ara(const struct device *dev, uint8_t *ara)
{
	const  struct pdc_config_t *cfg = dev->config;
	uint8_t buf = 0x0C;

	return i2c_write_read(cfg->i2c.bus, cfg->i2c.addr,
		&buf, 1,
		ara, 1);
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

	print_current_state(data);
	data->i2c_transaction_retry_counter = 0;
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
		if (data->i2c_transaction_retry_counter >
					N_I2C_TRANSACTION_COUNT) {
			/* MAX I2C transactions exceeded */
			/* TODO: handle the error */
			printk("I2C transaction failed\n");
		}
		return;
	}

	/* I2C transaction succeeded */
	set_state(data, ST_WAIT);
}

static void st_wait_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
	data->i2c_transaction_retry_counter = 0;
	data->ping_retry_counter = 0;
	data->ping_status.raw_value = 0;
}

static void st_wait_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	int rv;

	rv = get_ping_status(data->dev);
	if (rv < 0) {
		/* I2C transaction failed */
		data->i2c_transaction_retry_counter++;
		if (data->i2c_transaction_retry_counter >
					N_I2C_TRANSACTION_COUNT) {
			/* MAX I2C transactions exceeded */
			/* TODO: handle the error */
			LOG_ERR("Couldn't read Ping Status");
		}
		return;
	}

	switch (data->ping_status.cmd_sts) {
	case CMD_BUSY:
		LOG_INF("busy");
		/*
		 * The command has not been processed.
		 * Stay here and resend get ping status.
		 */
		data->ping_retry_counter++;
		if (data->ping_retry_counter > N_RETRY_COUNT) {
			/* MAX Ping Retries exceeded */
			/* TODO: handle the error */
			LOG_ERR("PDC not responding");
		}
		break;
	case CMD_DONE:
		LOG_INF("ping_status: %02x", data->ping_status.raw_value);
		/* Command completed successfully */
		if (data->ping_status.data_len > 0) {
			/* Data is available, so read it */
			set_state(data, ST_READ);
		} else {
			set_state(data, ST_IDLE);
		}
		break;
	case CMD_DEFERRED:
		LOG_INF("deferred");
		/*
		 * The command is currently being processed.
		 * Stay here and resend get ping status.
		 */
		data->ping_retry_counter++;
		if (data->ping_retry_counter > N_RETRY_COUNT) {
			/* MAX Ping Retries exceeded */
			/* TODO: handle the error */
			LOG_ERR("PDC not responding");
		}
		break;
	case CMD_ERROR:
		LOG_ERR("PDC command error");
		/* The command completed with an error. */
		set_state(data, ST_GET_ERROR_STATUS);
		break;
	}
}

static void st_read_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);
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
		 * TODO: Handle error
		 */
		/* ERROR READING DATA */
		printk("READ ERROR\n");
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

	/*
	 * TODO: user data might need further processing before copying
	 */
	/* Copy the received data to the user's buffer */
	memcpy(data->user_buf, data->rd_buf + offset, len);

	/* Clear the read buffer */
	memset(data->rd_buf, 0, 256);

	/* All done, return to idle state */
	set_state(data, ST_IDLE);
}

/* TODO: implement error handling */
static void st_error_entry(void *o)
{
}

static void st_error_run(void *o)
{
}

/* TODO: implement get error status */
static void st_get_error_status_entry(void *o)
{
}

static void st_get_error_status_run(void *o)
{
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
	[ST_WAIT] = SMF_CREATE_STATE(
		st_wait_entry,
		st_wait_run,
		NULL,
		NULL),
	[ST_READ] = SMF_CREATE_STATE(
		st_read_entry,
		st_read_run,
		NULL,
		NULL),
	[ST_ERROR] = SMF_CREATE_STATE(
		st_error_entry,
		st_error_run,
		NULL,
		NULL),
	[ST_GET_ERROR_STATUS] = SMF_CREATE_STATE(
		st_get_error_status_entry,
		st_get_error_status_run,
		NULL,
		NULL),
};

static void trigger_handler(struct k_work *work)
{
	k_work_submit(work);
}

static int rts54_get_ic_status(const struct device *dev,
			uint8_t offset, uint8_t len, uint8_t *buf)
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
	data->cmd = CMD_GET_IC_STATUS;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_rtk_status(const struct device *dev, uint8_t offset,
				uint8_t len, enum cmd_t cmd, uint8_t *buf)
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

static int rts54_set_handler_cb(const struct device *dev,
				pdc_cci_handler_cb_t cci_cb)
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

static int rts54_cancel(const struct device *dev)
{
	/* TODO */
	return 0;
}

static int rts54_connector_reset(const struct device *dev,
				enum connector_reset_t type)
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
	data->cmd = CMD_PPM_RESET;

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

static int rts54_set_notification_enable(const struct device *dev,
			union notification_enable_t bits, uint16_t ext_bits)
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

static int rts54_get_capability(const struct device *dev,
				struct device_capability_t *caps)
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

static int rts54_get_connector_capability(const struct device *dev,
					union connector_capability_t *caps)
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

static int rts54_get_connector_status(const struct device *dev,
				struct connector_status_t *cs)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if (cs == NULL) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x12;
	data->wr_buf[3] = 0x00;
	data->wr_buf_len = 4;
	data->user_buf = (uint8_t *)cs;
	data->cmd = CMD_GET_CONNECTOR_STATUS;

	k_mutex_unlock(&data->mtx);

	return 0;
}

static int rts54_get_error_status(const struct device *dev,
				struct error_status_t *es)
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

static int rts54_get_pdo(const struct device *dev, enum pdo_type_t pdo_type,
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

	byte4 = (num_pdos << 4) |
		(pdo_offset << 2) |
		(port_partner_pdo << 1) |
		pdo_type;

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x02;
	data->wr_buf[2] = 0x10;
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

	return rts54_get_ic_status(dev, 0, 1, is_flash_code);
}

static int rts54_get_fw_version(const struct device *dev, uint32_t *fw_version)
{
	if (fw_version == NULL) {
		return -EINVAL;
	}

	*fw_version = 0;
	return rts54_get_ic_status(dev, 3, 3, (uint8_t *)fw_version);
}

static int rts54_get_vid_pid(const struct device *dev, uint32_t *vid_pid)
{
	if (vid_pid == NULL) {
		return -EINVAL;
	}

	return rts54_get_ic_status(dev, 9, 4, (uint8_t *)vid_pid);
}

static int rts54_get_pd_version(const struct device *dev, uint32_t *pd_version)
{
	if (pd_version == NULL) {
		return -EINVAL;
	}

	return rts54_get_ic_status(dev, 22, 4, (uint8_t *)pd_version);
}

static int rts54_get_vbus_voltage(const struct device *dev, uint16_t *voltage)
{
	if (voltage == NULL) {
		return -EINVAL;
	}

	return rts54_get_rtk_status(dev, 16, 2, CMD_GET_VBUS_VOLTAGE,
				(uint8_t *)voltage);
}

static int rts54_get_vbus_current(const struct device *dev, uint16_t *current)
{
	if (current == NULL) {
		return -EINVAL;
	}

	return rts54_get_rtk_status(dev, 14, 2, CMD_GET_VBUS_CURRENT,
				(uint8_t *)current);
}

static int rts54_set_ccom(const struct device *dev, enum ccom_t ccom,
			enum drp_mode_t dm)
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

static int rts54_get_alternate_mode(const struct device *dev, enum sop_t sop,
				uint8_t alt_mode_offset, uint8_t num_alt_modes,
				struct alt_mode_t *alt_modes)
{
	struct pdc_data_t *data = dev->data;

	if (get_state(data) != ST_IDLE) {
		return -EBUSY;
	}

	if ((num_alt_modes > 3) || (alt_modes == NULL)) {
		return -EINVAL;
	}

	k_mutex_lock(&data->mtx, K_FOREVER);

	data->wr_buf[0] = 0x0E;
	data->wr_buf[1] = 0x05;
	data->wr_buf[2] = 0x0C;
	data->wr_buf[3] = 0x00;
	data->wr_buf[4] = sop;
	data->wr_buf[5] = alt_mode_offset;
	data->wr_buf[6] = num_alt_modes;
	data->wr_buf_len = 7;
	data->user_buf = (uint8_t *)alt_modes;
	data->cmd = CMD_GET_ALTERNATE_MODE;

	k_mutex_unlock(&data->mtx);

	return 0;

}

static int rts54_get_current_pdo(const struct device *dev, uint32_t *pdo)
{
	return 0;
}

static const struct pdc_driver_api_t pdc_driver_api = {
	.enable = rts54_enable,
	.get_ucsi_version = rts54_get_ucsi_version,
	.reset = rts54_reset,
	.cancel = rts54_cancel,
	.connector_reset = rts54_connector_reset,
	.set_notification_enable = rts54_set_notification_enable,
	.get_capability = rts54_get_capability,
	.get_connector_capability = rts54_get_connector_capability,
	.set_ccom = rts54_set_ccom,
	.set_uor = rts54_set_uor,
	.set_pdr = rts54_set_pdr,
	.set_sink_path = rts54_set_sink_path,
	.get_connector_status = rts54_get_connector_status,
	.get_pdo = rts54_get_pdo,
	.get_error_status = rts54_get_error_status,
	.get_alternate_mode = rts54_get_alternate_mode,
	.get_vbus_voltage = rts54_get_vbus_voltage,
	.get_vbus_current = rts54_get_vbus_current,
	.get_current_pdo = rts54_get_current_pdo,
	.set_handler_cb = rts54_set_handler_cb,

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

	/* TODO: add functionality */
}

static void pdc_interrupt_callback(const struct device *dev,
				struct gpio_callback *cb, uint32_t pins)
{
	struct pdc_data_t *data =
		CONTAINER_OF(cb,  struct pdc_data_t, gpio_cb);

	trigger_handler(&data->work);
}

static void run_driver(void *dev, void *unused1, void *unused2)
{
	struct pdc_data_t *data = ((const struct device *)dev)->data;

	while (1) {
		smf_run_state(SMF_CTX(data));
		k_sleep(K_MSEC(T_PING_STATUS));
	}
}

static int pdc_init(const struct device *dev)
{
	const  struct pdc_config_t *cfg = dev->config;
	struct pdc_data_t *data = dev->data;
	int rv;

	rv = i2c_is_ready_dt(&cfg->i2c);
	if (rv < 0) {
		LOG_ERR("I2C is not ready\n");
		return -ENODEV;
	}

	rv = gpio_is_ready_dt(&cfg->irq_gpios);
	if (rv < 0) {
		LOG_ERR("Interrupt GPIO is not ready\n");
		return -ENODEV;
	}

	rv = gpio_pin_configure_dt(&cfg->irq_gpios, GPIO_INPUT);
	if (rv < 0) {
		LOG_ERR("Unable to configure GPIO");
		return rv;
	}

	gpio_init_callback(&data->gpio_cb, pdc_interrupt_callback,
			BIT(cfg->irq_gpios.pin));

	rv = gpio_add_callback(cfg->irq_gpios.port, &data->gpio_cb);
	if (rv < 0) {
		LOG_ERR("Unable to add callback");
		return rv;
	}

	rv = gpio_pin_interrupt_configure_dt(&cfg->irq_gpios,
					GPIO_INT_EDGE_FALLING);
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

static void create_thread(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;

	data->thread = k_thread_create(
		&data->thread_data, thread_stack_area,
		K_THREAD_STACK_SIZEOF(thread_stack_area), run_driver,
		(void *)dev, 0, 0, 8, K_ESSENTIAL, K_NO_WAIT);
}

#define PDC_DEFINE(inst)                                                     \
	static  struct pdc_data_t pdc_data_##inst;                           \
                                                                             \
	static const  struct pdc_config_t pdc_config##inst = {               \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                           \
		.irq_gpios = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),         \
		.create_thread = create_thread,                              \
	};                                                                   \
                                                                             \
	DEVICE_DT_INST_DEFINE(inst, pdc_init, NULL,                          \
			      &pdc_data_##inst, &pdc_config##inst,           \
			      POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY, \
			      &pdc_driver_api);

DT_INST_FOREACH_STATUS_OKAY(PDC_DEFINE)
