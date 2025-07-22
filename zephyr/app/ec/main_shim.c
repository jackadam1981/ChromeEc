/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "drivers/ucsi_v3.h"
#include "ec_app_main.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "host_command.h"

#include <assert.h>
#include <drivers/pdc.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/smbus.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(usbc_pdc);

#define DT_DRV_COMPAT ite_it52xx_pdc

#define I2C_DEVICE_ADDR 0x40 /* i2c: 7b'0x40 */
#define TCPC_DEVICE_ADDR 0x26 /* TCPC: 7b'0x26 (port0 and port1) */
#define TIMEOUT_MS 20

#define I2C_NODE_ID DT_NODELABEL(i2c1)
#define PDC_POWER_P0_NODE_ID DT_NODELABEL(pdc_power_p0)

#define ITE_UCSI_CCI_COMMAND 0x80
/* CCI byte count and 4bytes data  */
#define PDC_MAX_CCI_LENGTH 0x05

bool ucsi_address;
//static struct gpio_dt_spec
//	it52xx_irq_list[1/*DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)*/];
struct gpio_callback gpio_cb;

struct smbus_cmd_t {
	/* Command ex. CTRL, MSGOUT, CCI, MSGIN */
	uint8_t cmd;
	/* Byte count */
	uint8_t len;
	/* Sub-Command */
	uint8_t sub;
};
static const struct smbus_cmd_t CLR_ALERT_STATUS = { 0xBC, 0x02 };
//static const struct smbus_cmd_t GET_IC_STATUS = { 0xB6, 0x02 };
static const struct smbus_cmd_t ITE_UCSI_PPM_RESET = { 0x81, 0x08, 0x01 };
//static const struct smbus_cmd_t SET_NOTIFICATION_ENABLE = { 0x81, 0x08, 0x05 };

/**
 * @brief PDC Config object
 */
struct pdc_config_t {
	/** I2C config */
	struct i2c_dt_spec i2c;
	/** pdc power path interrupt */
	struct gpio_dt_spec irq_gpios;
	/** connector number of this port */
	uint8_t connector_number;
	/** Notification enable bits */
	union notification_enable_t bits;
	/** Create thread function */
	void (*create_thread)(const struct device *dev);
	/** If true, do not apply PDC FW updates to this port */
	bool no_fw_update;
	/** Whether or not this port supports CCD */
	bool ccd;
	/** Pointer to the device-specific callback function */
	gpio_callback_handler_t callback_handler;
};

enum cmd_t {
	/** No command */
	CMD_NONE,
	/** CMD_TRIGGER_PDC_RESET */
	CMD_TRIGGER_PDC_RESET,
	/** PDC Enable */
	CMD_VENDOR_ENABLE,
	/** Set Notification Enable */
	CMD_SET_NOTIFICATION_ENABLE,
	/** PDC Reset */
	CMD_PPM_RESET,
	/** Connector Reset */
	CMD_CONNECTOR_RESET,
	/** Get Capability */
	CMD_GET_CAPABILITY,
	/** Get Connector Capability */
	CMD_GET_CONNECTOR_CAPABILITY,
	/** Set UOR */
	CMD_SET_UOR,
	/** Set PDR */
	CMD_SET_PDR,
	/** Get PDOs */
	CMD_GET_PDOS,
	/** Get Connector Status */
	CMD_GET_CONNECTOR_STATUS,
	/** Get Error Status */
	CMD_GET_ERROR_STATUS,
	/** Get VBUS Voltage */
	CMD_GET_VBUS_VOLTAGE,
	/** Get IC Status */
	CMD_GET_IC_STATUS,
	/** Set CCOM */
	CMD_SET_CCOM,
	/** Set DRP_MODE */
	CMD_SET_DRP_MODE,
	/** Get DRP_MODE */
	CMD_GET_DRP_MODE,
	/** Read Power Level */
	CMD_READ_POWER_LEVEL,
	/** Get RDO */
	CMD_GET_RDO,
	/** Set RDO */
	CMD_SET_RDO,
	/** Set Sink Path */
	CMD_SET_SINK_PATH,
	/** Get current Partner SRC PDO */
	CMD_GET_CURRENT_PARTNER_SRC_PDO,
	/** Set the Fast Role Swap */
	CMD_SET_FRS_FUNCTION,
	/** Set the Rp TypeC current */
	CMD_SET_TPC_RP,
	/** TypeC reconnect */
	CMD_SET_TPC_RECONNECT,
	/** set Retimer into FW Update Mode */
	CMD_SET_RETIMER_FW_UPDATE_MODE,
	/** Get the cable properties */
	CMD_GET_CABLE_PROPERTY,
	/** Get VDO(s) of PDC, Cable, or Port partner */
	CMD_GET_VDO,
	/** CMD_GET_IDENTITY_DISCOVERY */
	CMD_GET_IDENTITY_DISCOVERY,
	/** CMD_GET_IS_VCONN_SOURCING */
	CMD_GET_IS_VCONN_SOURCING,
	/** CMD_SET_PDO */
	CMD_SET_PDO,
	/** Get PDC ALT MODE Status Register value */
	CMD_GET_PCH_DATA_STATUS,
	/** CMD_ACK_CC_CI */
	CMD_ACK_CC_CI,
	/** Raw UCSI call.
	 * Special handling of the data read from a PDC will be skipped. */
	CMD_RAW_UCSI,
	/** CMD_GET_LPM_PPM_INFO */
	CMD_GET_LPM_PPM_INFO,
	/** CMD_GET_ATTENTION_VDO */
	CMD_GET_ATTENTION_VDO,
	/** CMD_GET_SBU_MUX_MODE */
	CMD_GET_SBU_MUX_MODE,
	/** CMD_SET_SBU_MUX_MODE */
	CMD_SET_SBU_MUX_MODE,
	/** Set the Burnside Bridge retimer into CTS test mode */
	CMD_SET_BBR_CTS,
	/** Clear it52xx alert status */
	CMD_CLR_ALERT_STATUS,
};

union ping_status_t {
	/* rtk cci only reply one byte */
	//struct {
	//	/** Command status */
	//	uint8_t cmd_sts : 2;
	//	/** Length of data read to read */
	//	uint8_t data_len : 6;
	//};
	//uint8_t raw_value;

	/* ite(spec) cci reply five bytes ByteCount - xx xx(MSGIN len) xx xx(status: ... /busy/ACK/error/CC) */
	struct {
		/** Byte count of CCI Command */
		uint8_t cci_byte_cnt;
		/** End of message indicator */
		uint8_t end_of_msgi : 1;
		/** Connector change indicator */
		uint8_t cci : 7;
		/** Data length */
		uint8_t data_len;
		/** Vendor defined message indicator */
		uint8_t vdmi : 1;
		/** Reserved */
		uint8_t Reserved : 6;
		/** Security request indicator */
		uint8_t sri : 1;
		/** Command status */
		uint8_t cmd_sts;
	};
	uint8_t raw_value[PDC_MAX_CCI_LENGTH];
};

/**
 * @brief PDC Data object
 */
struct pdc_data_t {
	/** State machine context */
	//struct smf_ctx ctx;
	/** Init's local state variable */
	//enum init_state_t init_local_state;
	/** Init's current state */
	//enum init_state_t init_local_current_state;
	/** Init's next state */
	//enum init_state_t init_local_next_state;
	/** PDC's last state */
	//enum state_t last_state;
	/** PDC device structure */
	const struct device *dev;
	/** PDC command */
	enum cmd_t cmd;
	/** Driver thread */
	//k_tid_t thread;
	/** Driver thread's data */
	//struct k_thread thread_data;
	/** Ping status */
	union ping_status_t ping_status;
	/** Timepoint for when we can next call ping status. */
	k_timepoint_t next_ping_status;
	/** Ping status retry counter */
	uint8_t ping_retry_counter;
	/** Number of time the init process has been attempted */
	uint8_t init_retry_counter;
	/** I2C retry counter */
	uint8_t i2c_transaction_retry_counter;
	/** PDC write buffer */
	uint8_t wr_buf[PDC_MAX_DATA_LENGTH];
	/** Length of bytes in the write buffer */
	uint8_t wr_buf_len;
	/** PDC read buffer */
	uint8_t rd_buf[PDC_MAX_DATA_LENGTH];
	/** Length of bytes in the read buffer */
	uint8_t rd_buf_len;
	/** Pointer to user data */
	uint8_t *user_buf;
	/** Command mutex */
	//struct k_mutex mtx;
	/** GPIO interrupt callback */
	struct gpio_callback gpio_cb;
	/** Error status */
	//union error_status_t error_status;
	/** CCI Event */
	union cci_event_t cci_event;
	/** CC Event callback */
	struct pdc_callback *cc_cb;
	/** CC Event one-time callback. If it's NULL, cci_cb will be called. */
	struct pdc_callback *cc_cb_tmp;
	/** Asynchronous (CI) Event callbacks */
	//sys_slist_t ci_cb_list;
	/** Information about the PDC */
	//struct pdc_info_t info;
	/** Init done flag */
	bool init_done;
	/** Error recovery delay counter */
	uint16_t error_recovery_delay_counter;
	/** Error recovery counter */
	uint16_t error_recovery_counter;
	/** Error Status used during initialization */
	//union error_status_t es;
	/* Driver specific events to handle. */
	//struct k_event driver_event;
	/* Currently running UCSI command. */
	enum ucsi_command_t active_ucsi_cmd;
};

struct pdc_data_t data_0;
/**
 * @brief Helper method for setting up a command call.
 * @param dev PDC device pointer
 * @param cmd Command to execute
 * @param buf Command payload to copy into write buffer
 * @param len Length of paylaod buffer
 * @param user_buf Pointer to buffer where response data will be written.
 * @return 0 on success
 * @return -EBUSY if command is already pending.
 * @return -ECONNREFUSED if chip communication is disabled
 */
static int it52xx_post_command_with_callback(/*const struct device *dev,*/
					    enum cmd_t cmd, const uint8_t *buf,
					    uint8_t len, uint8_t *user_buf,
					    struct pdc_callback *callback)
{
	struct pdc_data_t *data = &data_0 /*dev->data*/;

	/* Return an error if chip communication is suspended */
	//if (check_comms_suspended()) {
	//	return -ECONNREFUSED;
	//}

	//k_mutex_lock(&data->mtx, K_FOREVER);

	//if (data->cmd != CMD_NONE) {
	//	k_mutex_unlock(&data->mtx);
	//	return -EBUSY;
	//}

	if (buf) {
		assert(len <= sizeof(data->wr_buf));
		memcpy(data->wr_buf, buf, len);
	}

	data->wr_buf_len = len;
	data->user_buf = user_buf;
	data->cmd = cmd;
	data->cc_cb_tmp = callback;

	//LOG_ERR("buf[0x%x] = { 0x%x, 0x%x, 0x%x, 0x%x }  ", len, buf[0], buf[1], buf[2], buf[3]);
	//LOG_ERR("data->wr_buf[0x%x] = { 0x%x, 0x%x, 0x%x, 0x%x }  ", data->wr_buf_len, data->wr_buf[0], data->wr_buf[1], data->wr_buf[2], data->wr_buf[3]);
	/* If sending a raw UCSI command, byte[2] is the actual UCSI command
	 * being executed.
	 */
	if (cmd == CMD_RAW_UCSI && buf) {
		data->active_ucsi_cmd = data->wr_buf[2];
	}

	//if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG)) {
	//	const struct pdc_config_t *cfg = dev->config;

	//	pdc_trace_msg_req(cfg->connector_number,
	//			  PDC_TRACE_CHIP_TYPE_RTS54XX, data->wr_buf,
	//			  data->wr_buf_len);
	//}

	//k_mutex_unlock(&data->mtx);
	/* Posting the event reduces latency to start executing the command. */
	//k_event_post(&data->driver_event, IT52XX_NEXT_STATE_READY);

	return 0;
}

static int it52xx_post_command(/*const struct device *dev,*/ enum cmd_t cmd,
			      const uint8_t *buf, uint8_t len,
			      uint8_t *user_buf)
{
	return it52xx_post_command_with_callback(/*dev,*/ cmd, buf, len, user_buf,
						NULL);
}

static int it52xx_clr_alert_status(void /*const struct device *dev*/)
{
	//struct pdc_data_t *data = dev->data;

	/* Can only be called from Init State */
	//if (get_state(data) != ST_INIT) {
	//	return -EBUSY;
	//}

	uint8_t payload[] = {
		CLR_ALERT_STATUS.cmd,
		CLR_ALERT_STATUS.len,
	};

	return it52xx_post_command(/*dev,*/ CMD_CLR_ALERT_STATUS, payload,
				  ARRAY_SIZE(payload), NULL);
}

#if 0
static int it52xx_get_info(const struct device *dev, struct pdc_info_t *info,
			  bool live)
{
	const struct pdc_config_t *cfg = dev->config;
	struct pdc_data_t *data = dev->data;

	if (info == NULL) {
		return -EINVAL;
	}

	/* If caller is OK with a non-live value and we have one, we can
	 * immediately return a cached value.
	 */
	if (!live) {
	//	k_mutex_lock(&data->mtx, K_FOREVER);

		/* Check FW ver and VID/PID fields for valid values to ensure
		 * we have a resident value.
		 */
	//	if (data->info.fw_version == PDC_FWVER_INVALID ||
	//	    data->info.vid == PDC_VID_INVALID ||
	//	    data->info.pid == PDC_PID_INVALID) {
	//		k_mutex_unlock(&data->mtx);

			/* No cached value. Caller should request a live read */
	//		return -EAGAIN;
	//	}

	//	*info = data->info;
	//	k_mutex_unlock(&data->mtx);

	//	LOG_DBG("C%d: Use cached chip info (%u.%u.%u)",
	//		cfg->connector_number,
	//		PDC_FWVER_GET_MAJOR(data->info.fw_version),
	//		PDC_FWVER_GET_MINOR(data->info.fw_version),
	//		PDC_FWVER_GET_PATCH(data->info.fw_version));
	//	return 0;
	}

	/* Handle a live read */

	//if ((get_state(data) != ST_IDLE) && (get_state(data) != ST_INIT)) {
	//	return -EBUSY;
	//}

	/* Post a command and perform a chip operation */
	uint8_t payload[] = {
		GET_IC_STATUS.cmd, GET_IC_STATUS.len, 0, 0x00, 38,
	};

	LOG_DBG("Get chip info");

	return rts54_post_command(dev, CMD_GET_IC_STATUS, payload,
				  ARRAY_SIZE(payload), (uint8_t *)info);
}
#endif

static int it52xx_ppm_reset(void /*const struct device *dev*/)
{
	//struct pdc_data_t *data = dev->data;

	/* Can only be called from Init State */
	//if (get_state(data) != ST_INIT) {
	//	return -EBUSY;
	//}

	uint8_t payload[] = {
		ITE_UCSI_PPM_RESET.cmd,
		ITE_UCSI_PPM_RESET.len,
		ITE_UCSI_PPM_RESET.sub,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
		0x00,
	};

	return it52xx_post_command(/*dev,*/ CMD_PPM_RESET, payload,
				  ARRAY_SIZE(payload), NULL);
}

#if 0
static void st_write_entry(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;

	print_current_state(data);

	/* This state can only be entered from the Init and Idle states */
	assert(data->last_state == ST_INIT || data->last_state == ST_IDLE);

	/* Clear I2C transaction retry counter */
	data->i2c_transaction_retry_counter = 0;
	/* Only clear Error Status if the subsystem isn't going to read it */
	if (data->cmd != CMD_GET_ERROR_STATUS) {
		/* Clear the Error Status */
		data->error_status.raw_value = 0;
	}
	/* Clear the CCI Event */
	data->cci_event.raw_value = 0;
}
#endif

static int it52xx_i2c_write(/*const struct device *dev,*/
			    const struct i2c_dt_spec *i2c_dt)
{
	struct pdc_data_t *data = &data_0 /*dev->data*/;
	/* const struct pdc_config_t *cfg = dev->config;*/
	struct i2c_msg msg;

	msg.buf = data->wr_buf;
	msg.len = data->wr_buf_len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(i2c_dt /*&cfg->i2c*/, &msg, 1);
}

#if 0
static enum smf_state_result st_write_run(void *o)
{
	struct pdc_data_t *data = (struct pdc_data_t *)o;
	int rv;

	/* Write the command */
	rv = it52xx_i2c_write(data->dev);
	if (rv < 0) {
		if (max_i2c_retry_reached(data, I2C_MSG_WRITE)) {
			set_state(data, ST_ERROR_RECOVERY);
		}
		return SMF_EVENT_HANDLED;
	}

	/* I2C transaction succeeded. Set timepoint for next ping status. */
	data->next_ping_status = sys_timepoint_calc(K_MSEC(T_PING_STATUS));
	set_state(data, ST_PING_STATUS);

	return SMF_EVENT_HANDLED;
}
#endif

static int get_ping_status(/*const struct device *dev,*/
			   const struct i2c_dt_spec *i2c_dt)
{
	//struct pdc_data_t *data = dev->data;
	//const struct pdc_config_t *cfg = dev->config;
	//struct i2c_msg msg;

	/* only read 1 byte CCI */
	//msg.buf = &data->ping_status.raw_value;
	//msg.len = 1;
	//msg.flags = I2C_MSG_READ | I2C_MSG_STOP;

	//return i2c_transfer_dt(&cfg->i2c, &msg, 1);

	struct pdc_data_t *data = &data_0 /*dev->data*/;
	/* const struct pdc_config_t *cfg = dev->config; */
	struct i2c_msg msg[2];
	uint8_t cmd = ITE_UCSI_CCI_COMMAND;

	msg[0].buf = &cmd;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = data->ping_status.raw_value; /* original 1byte, we need 5byte space */
	msg[1].len = 5; /* W CCI - R [04 xx xx xx xx] => len = 5 */
	msg[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;

	return i2c_transfer_dt(i2c_dt /*&cfg->i2c*/, msg, 2);
}

#if 0
static int it52xx_i2c_read(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	const struct pdc_config_t *cfg = dev->config;
	struct i2c_msg msg[2];
	uint8_t cmd = RTS54XX_BLOCK_READ_CMD;
	int rv;

	msg[0].buf = &cmd;
	msg[0].len = 1;
	msg[0].flags = I2C_MSG_WRITE;

	msg[1].buf = data->rd_buf;
	msg[1].len = data->ping_status.data_len + 1;
	msg[1].flags = I2C_MSG_RESTART | I2C_MSG_READ | I2C_MSG_STOP;

	//rv = i2c_transfer_dt(&cfg->i2c, msg, 2);
	if (ucsi_address) {
		rv = i2c_transfer(&cfg->i2c, msg, 2, TCPC_DEVICE_ADDR);
	} else {
		rv = i2c_transfer(&cfg->i2c, msg, 2, I2C_DEVICE_ADDR);
	}
	if (rv < 0) {
		return rv;
	}

	data->rd_buf_len = data->ping_status.data_len;

	if (IS_ENABLED(CONFIG_USBC_PDC_TRACE_MSG)) {
		pdc_trace_msg_resp(cfg->connector_number,
				   PDC_TRACE_CHIP_TYPE_RTS54XX, data->rd_buf,
				   data->ping_status.data_len + 1);
	}

	return rv;
}
#endif

#if 0
/**
 * @brief This function should be called after any I2C transfer that failed.
 * It increments a counter, notifies the subsystem of the I2C error and then
 * enters the recovery state. NOTE: the data->i2c_transaction_retry_counter
 * should be set to zero in the calling states entry action.
 */
static bool max_i2c_retry_reached(struct pdc_data_t *data, int type)
{
	const struct pdc_config_t *cfg = data->dev->config;

	data->i2c_transaction_retry_counter++;
	if (data->i2c_transaction_retry_counter > N_I2C_TRANSACTION_COUNT) {
		/* MAX I2C transactions exceeded */
		LOG_ERR("C%d: %s i2c error", cfg->connector_number,
			(type & I2C_MSG_READ) ? "Read" : "Write");
		/*
		 * The command was not successfully completed,
		 * so set cci.error to 1b.
		 */
		data->cci_event.error = 1;
		/* Command has completed */
		data->cci_event.command_completed = 1;
		/* Clear busy event */
		data->cci_event.busy = 0;
		/* Set error, I2C read error */
		if (type & I2C_MSG_READ) {
			data->error_status.i2c_read_error = 1;
		} else {
			data->error_status.i2c_write_error = 1;
		}
		/* Notify system of status change */
		call_cci_event_cb(data);
		return true;
	}
	return false;
}
#endif

static void pdc_interrupt_callback(const struct device *dev,
				   struct gpio_callback *cb,
				   uint32_t pins)
{
	//k_event_post(&PDC_DATA_STRUCT_NAME(inst).driver_event,
	//	     RTS54XX_IRQ_EVENT);
	//read 0xBD
	LOG_ERR("pdc interrupt callback");
}

/** A stub main to call the real ec app main function. LCOV_EXCL_START */
int main(void)
{
	const struct i2c_dt_spec i2c_dt = { I2C_DT_SPEC_GET_ON_I2C(PDC_POWER_P0_NODE_ID) }; /*Get PDC_POWER_P0_NODE is belong which i2c node(bus) & reg of PDC_POWER_P0_NODE(address)*/
	const struct gpio_dt_spec irq_gpios_dt = { .port = DEVICE_DT_GET(DT_NODELABEL(gpioa)),
						   .pin = 6,
						   .dt_flags = GPIO_ACTIVE_LOW };
	/*GPIO_DT_SPEC_INST_GET(0, irq_gpios); => __device_dts_ord undeclared*/
	/*GPIO_DT_SPEC_GET(PDC_POWER_P0_NODE_ID, irq_gpios); => __device_dts_ord undeclared*/
	/*GPIO_DT_SPEC_GET_BY_IDX(PDC_POWER_P0_NODE_ID, irq_gpios, 0); => __device_dts_ord undeclared*/
	/*Get port & pin & flag of irq-gpios prop (within PDC_POWER_P0_NODE)*/
	//bool irq_init_done = false;
	/* PDC device structure */
	//const struct device *dev = DEVICE_DT_GET(PDC_POWER_P0_NODE_ID); /*=> undefined reference to `__device_dts_ord_371 */
	int rv;

	ec_app_main();

	/* i2c test code (refer to i2c test & pdc driver) */
	rv = i2c_is_ready_dt(&i2c_dt /*&cfg->i2c*/);
	if (rv < 0) {
		LOG_ERR("device %s not ready", i2c_dt.bus->name /*cfg->i2c.bus->name*/);
		return -ENODEV;
	}

	rv = gpio_is_ready_dt(&irq_gpios_dt /*&cfg->irq_gpios*/);
	if (rv < 0) {
		LOG_ERR("device %s not ready", irq_gpios_dt.port->name /*cfg->irq_gpios.port->name*/);
		return -ENODEV;
	}

	//k_event_init(&data->driver_event);

	//for (int i = 0; i < ARRAY_SIZE(it52xx_irq_list); i++) {
	//	if (it52xx_irq_list[i].port == irq_gpios_dt.port/*cfg->irq_gpios.port*/ &&
	//	    it52xx_irq_list[i].pin == irq_gpios_dt.pin/*cfg->irq_gpios.pin*/) {
	//		irq_init_done = true;
	//		break;
	//	}

	//	if (it52xx_irq_list[i].port == NULL) {
	//		it52xx_irq_list[i] = irq_gpios_dt /*cfg->irq_gpios*/;
	//		break;
	//	}
	//}

	//if (!irq_init_done) {
		rv = gpio_pin_configure_dt(&irq_gpios_dt /*&cfg->irq_gpios*/, GPIO_INPUT);
		if (rv < 0) {
			LOG_ERR("Unable to configure GPIO");
			return rv;
		}

#if 0
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_gpio_ec_i2c_pdc0_sm_int_odl));
		//but need to declare in interrupt.dts:
		//gpio-interrupts {
		//      compatible = "cros-ec,gpio-interrupts";
		//
		//      int_gpio_ec_i2c_pdc0_sm_int_odl: gpio_ec_i2c_pdc0_sm_int_odl {
		//          irq-pin = <&gpio_ec_i2c_pdc0_sm_int_odl>;
		//          flags = <GPIO_INT_EDGE_FALLING>;
		//          handler = "pdc_interrupt_callback";
		//      };
		//};
		/* 'pdc_interrupt_callback' defined but not used */
#else
		gpio_init_callback(&gpio_cb /*&data->gpio_cb*/, pdc_interrupt_callback /*cfg->callback_handler*/,
				   BIT(irq_gpios_dt.pin /*cfg->irq_gpios.pin*/));

		rv = gpio_add_callback(irq_gpios_dt.port /*cfg->irq_gpios.port*/, &gpio_cb /*&data->gpio_cb*/);
		if (rv < 0) {
			LOG_ERR("Unable to add callback");
			return rv;
		}

		rv = gpio_pin_interrupt_configure_dt(&irq_gpios_dt /*&cfg->irq_gpios*/,
						     GPIO_INT_EDGE_FALLING);
		if (rv < 0) {
			LOG_ERR("Unable to configure interrupt");
			return rv;
		}
		LOG_ERR("init done");
#endif
		/* Trigger IRQ on startup to read any pending interrupts */
		//k_event_post(&data->driver_event, IT52XX_IRQ_EVENT);
	//}

	//test tx/rx i2c data

	//test tx/rx ucsi data
	k_sleep(K_MSEC(TIMEOUT_MS));

	it52xx_ppm_reset(/*dev*/);
	//[ST_WRITE]: st_write_entry(&pdc_data_0); -> st_write_run(&pdc_data_0);
	it52xx_i2c_write(/*dev,*/ &i2c_dt);
	//delay (still can read CCI if not read INT) or read INT
	k_sleep(K_MSEC(TIMEOUT_MS));

	//it52xx_clr_alert_status(/*dev*/);   //what happen if clear INT before read CCI?
	//it52xx_i2c_write(/*dev,*/ &i2c_dt); //reply the same CCI event (why?)

	//[PING_STATUS] = get CCI
	get_ping_status(/*dev,*/ &i2c_dt);

	//get_ping_status(/*dev,*/ &i2c_dt); //what happen if read CCI again & not clear INT?
	                                     //reply the same CCI event (need clear INT to elimate event)
	//if data len of CCI != 0 -> [ST_READ] = read MSGIN
	//if (data->ping_status.data_len > 0) {
		//it52xx_i2c_read(dev);
	//}
	//clear INT, do in [PING_STATUS] data == 0 & [ST_READ] data != 0
	it52xx_clr_alert_status(/*dev*/);
	it52xx_i2c_write(/*dev,*/ &i2c_dt);


	if (IS_ENABLED(CONFIG_TASK_HOSTCMD_THREAD_MAIN)) {
		host_command_main();
	} else if (IS_ENABLED(CONFIG_THREAD_MONITOR)) {
		/*
		 * Avoid returning so that the main stack is displayed by the
		 * "kernel stacks" shell command.
		 */
		k_sleep(K_FOREVER);
	}

	return 0;
}
/* LCOV_EXCL_STOP */
