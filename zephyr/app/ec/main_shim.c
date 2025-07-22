/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "host_command.h"

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

#define I2C_NODE_ID DT_NODELABEL(i2c1)
#define PDC_POWER_P0_NODE_ID DT_NODELABEL(pdc_power_p0)

bool ucsi_address;
//static struct gpio_dt_spec
//	it52xx_irq_list[1/*DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT)*/];
struct gpio_callback gpio_cb;

#if 0
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

/**
 * @brief PDC Data object
 */
struct pdc_data_t {
	/** State machine context */
	struct smf_ctx ctx;
	/** Init's local state variable */
	enum init_state_t init_local_state;
	/** Init's current state */
	enum init_state_t init_local_current_state;
	/** Init's next state */
	enum init_state_t init_local_next_state;
	/** PDC's last state */
	enum state_t last_state;
	/** PDC device structure */
	const struct device *dev;
	/** PDC command */
	enum cmd_t cmd;
	/** Driver thread */
	k_tid_t thread;
	/** Driver thread's data */
	struct k_thread thread_data;
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
	struct k_mutex mtx;
	/** GPIO interrupt callback */
	struct gpio_callback gpio_cb;
	/** Error status */
	union error_status_t error_status;
	/** CCI Event */
	union cci_event_t cci_event;
	/** CC Event callback */
	struct pdc_callback *cc_cb;
	/** CC Event one-time callback. If it's NULL, cci_cb will be called. */
	struct pdc_callback *cc_cb_tmp;
	/** Asynchronous (CI) Event callbacks */
	sys_slist_t ci_cb_list;
	/** Information about the PDC */
	struct pdc_info_t info;
	/** Init done flag */
	bool init_done;
	/** Error recovery delay counter */
	uint16_t error_recovery_delay_counter;
	/** Error recovery counter */
	uint16_t error_recovery_counter;
	/** Error Status used during initialization */
	union error_status_t es;
	/* Driver specific events to handle. */
	struct k_event driver_event;
	/* Currently running UCSI command. */
	enum ucsi_command_t active_ucsi_cmd;
};

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

static int it52xx_i2c_write(const struct device *dev)
{
	struct pdc_data_t *data = dev->data;
	const struct pdc_config_t *cfg = dev->config;
	struct i2c_msg msg;
	int rv;

	msg.buf = data->wr_buf;
	msg.len = data->wr_buf_len;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	if (ucsi_address) {
		rv = i2c_transfer(&cfg->i2c, &msg, 2, TCPC_DEVICE_ADDR);
	} else {
		rv = i2c_transfer(&cfg->i2c, &msg, 2, I2C_DEVICE_ADDR);
	}

	return rv;
	//return i2c_transfer_dt(&cfg->i2c, &msg, 1);
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
