/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Driver file for PD task to configure USB-C Alternate modes on Intel SoC.
 * Elaborate details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/intel_altmode.h>
#include <ap_power/ap_power.h>
#include "i2c.h"

#define DT_DRV_COMPAT intel_pd_altmode

LOG_MODULE_REGISTER(INTEL_ALTMODE, LOG_LEVEL_ERR);

struct pd_altmode_config {
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
	/* Shared interrupt pin in dual port solution */
	bool shared_irq;
};

struct pd_altmode_data {
	const struct device *dev;
	struct k_work work;
	struct gpio_callback gpio_cb;
	intel_altmode_callback isr_cb;
};

static int intel_altmode_read_status(const struct device *dev,
				     union data_status_reg *data)
{
	const struct pd_altmode_config *cfg = dev->config;
	uint8_t buf[INTEL_ALTMODE_DATA_STATUS_REG_LEN + 1];
	int rv;

	/*
	 * Read sequence
	 * DEV_ADDR - REG_ID - DEV_ADDR - READ_LEN - DATA0 .. DATAn
	 */
	rv = i2c_burst_read_dt(&cfg->i2c, INTEL_ALTMODE_REG_DATA_STATUS, buf,
			       INTEL_ALTMODE_DATA_STATUS_REG_LEN + 1);
	if (rv)
		return rv;
	if (buf[0] != INTEL_ALTMODE_DATA_STATUS_REG_LEN)
		return -EIO;

	memcpy(data, &buf[1], INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	return 0;
}

static int intel_altmode_write_control(const struct device *dev,
				       union data_control_reg *data)
{
	uint8_t buf[INTEL_ALTMODE_DATA_CONTROL_REG_LEN + 2];
	const struct pd_altmode_config *cfg = dev->config;
	struct i2c_msg msg;

	/*
	 * Write sequence
	 * DEV_ADDR - REG_ID - DATA_LEN - DATA0 .. DATAn
	 */
	buf[0] = INTEL_ALTMODE_REG_DATA_CONTROL;
	buf[1] = INTEL_ALTMODE_DATA_CONTROL_REG_LEN;
	memcpy(&buf[2], data->raw_value, INTEL_ALTMODE_DATA_CONTROL_REG_LEN);

	msg.buf = (uint8_t *)&buf;
	msg.len = INTEL_ALTMODE_DATA_CONTROL_REG_LEN + 2;
	msg.flags = I2C_MSG_WRITE | I2C_MSG_STOP;

	return i2c_transfer_dt(&cfg->i2c, &msg, 1);
}

static bool intel_altmode_is_interrupted(const struct device *dev)
{
	const struct pd_altmode_config *cfg = dev->config;

	return gpio_pin_get_dt(&cfg->int_gpio);
}

static void intel_altmode_set_result_cb(const struct device *dev,
					intel_altmode_callback cb)
{
	struct pd_altmode_data *data = dev->data;
	const struct pd_altmode_config *cfg = data->dev->config;

	if (!cfg->shared_irq) {
		data->isr_cb = cb;
	}
}

static const struct intel_altmode_driver_api intel_pd_altmode_driver_api = {
	.read_status = intel_altmode_read_status,
	.write_control = intel_altmode_write_control,
	.is_interrupted = intel_altmode_is_interrupted,
	.set_result_cb = intel_altmode_set_result_cb,
};

static void pd_altmode_gpio_callback(const struct device *dev,
				     struct gpio_callback *cb, uint32_t pins)
{
	struct pd_altmode_data *data =
		CONTAINER_OF(cb, struct pd_altmode_data, gpio_cb);
	const struct pd_altmode_config *cfg = data->dev->config;

	if (!cfg->shared_irq) {
		k_work_submit(&data->work);
	}
}

static void pd_altmode_isr_work(struct k_work *item)
{
	struct pd_altmode_data *data =
		CONTAINER_OF(item, struct pd_altmode_data, work);
	const struct pd_altmode_config *cfg = data->dev->config;

	if (!cfg->shared_irq) {
		data->isr_cb();
	}
}

typedef uint8_t* rt_smbus_cmd;

#define RT_SMBUS_CMD_RETRIES           10
#define RT_SMBUS_CMD_LEN(cmd)          cmd[1]
#define RT_SMBUS_CMD_RESULT(ping)      (ping & 3)
#define RT_SMBUS_CMD_RESULT_LEN(ping)  ((ping >> 2) & 0x3F)

static int rt_smbus_send_cmd(rt_smbus_cmd cmd, uint8_t *resp_len)
{
	uint8_t ping;
	int ping_retries;
	int rv;

	i2c_lock(I2C_PORT_TYPEC_AIC_1, 1);
	rv = i2c_xfer_unlocked(I2C_PORT_TYPEC_AIC_1, 0x69, cmd,
				RT_SMBUS_CMD_LEN(cmd) + 2, NULL, 0,
				I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_TYPEC_AIC_1, 0);
	if (rv) {
		return rv;
	}

	ping_retries = 0;
	k_msleep(3);
	do {
		k_msleep(3);
		i2c_lock(I2C_PORT_TYPEC_AIC_1, 1);
		rv = i2c_xfer_unlocked(I2C_PORT_TYPEC_AIC_1, 0x69, NULL, 0, &ping, 1,
				I2C_XFER_SINGLE);
		i2c_lock(I2C_PORT_TYPEC_AIC_1, 0);
		if (rv) {
			return rv;
		}
	} while ((RT_SMBUS_CMD_RESULT(ping)  == 0) && ping_retries++ < RT_SMBUS_CMD_RETRIES);

	if (RT_SMBUS_CMD_RESULT(ping)  != 1) {
		return -EPROTO;
	}

	if (resp_len) {
		*resp_len = RT_SMBUS_CMD_RESULT_LEN(ping);
	}

	return 0;
}

int rt_smbus_get_resp(uint8_t *resp, uint8_t len)
{
	int rv;
	uint8_t cmd = 0x80;

	i2c_lock(I2C_PORT_TYPEC_AIC_1, 1);
	rv = i2c_xfer_unlocked(I2C_PORT_TYPEC_AIC_1, 0x69, &cmd, 1, resp,
			len + 1,
			I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_TYPEC_AIC_1, 0);

	return rv;
}

#define RT_SMBUS_CMD_ENABLE_VENDOR     {0x01, 0x03, 0xda, 0x0b, 0x01}
#define RT_SMBUS_CMD_GET_TPC_OP_MODE   {0x08, 0x02, 0x9d, 0x00}
#define RT_SMBUS_CMD_SET_TPC_OP_MODE   {0x08, 0x03, 0x1d, 0x00, 0x00}
#define RT_SMBUS_CMD_SET_VOLTAGE       {0x1e, 0x03, 0x00, 0x00, 0x00}
#define RT_SMBUS_CMD_SET_NOTIF_ENABLE  {0x08, 0x06, 0x01, 0x00, 0x00, 0x00, 0x00}

static void enable_vendor(void)
{
	uint8_t cmd[] = RT_SMBUS_CMD_ENABLE_VENDOR;

	rt_smbus_send_cmd(cmd, NULL);
}

static void set_drm(bool enable)
{
	uint8_t cmd[] = RT_SMBUS_CMD_SET_TPC_OP_MODE;

	cmd[4] = enable? 0x05: 0x04;

	rt_smbus_send_cmd(cmd, NULL);
}

static void set_voltage(void)
{
	uint8_t cmd[] =  RT_SMBUS_CMD_SET_VOLTAGE;
	int rv;

	rv = rt_smbus_send_cmd(cmd, NULL);
	if (rv) {
		LOG_ERR("ERROR Gettin CSD OP Mode");
	}
}

void get_tcp_op_mode(void)
{
	uint8_t resp_len;
	uint8_t resp[16];
	uint8_t cmd[] =  RT_SMBUS_CMD_GET_TPC_OP_MODE;
	int rv;

	rv = rt_smbus_send_cmd(cmd, &resp_len);
	if (rv || resp_len == 0) {
		LOG_ERR("ERROR Gettin CSD OP Mode");
	}

	rt_smbus_get_resp(resp, resp_len);
}

static void init_drp(void)
{

	enable_vendor();

	set_drm(false);

	/* Enable this code to get default TPC Operation mode */
	//get_tcp_op_mode();
}

void pd_driver_hook(struct ap_power_ev_callback *cb,
					  struct ap_power_ev_data data)
{
	switch (data.event) {
	case AP_POWER_RESUME:
		set_drm(true);
		break;
	case AP_POWER_PRE_INIT:
		set_voltage();
		break;

	default:
	}
}

static void intel_altmode_init_cbs(void)
{
	static struct ap_power_ev_callback cb;

	ap_power_ev_init_callback(&cb,
				  pd_driver_hook,
				  AP_POWER_RESUME | AP_POWER_PRE_INIT);
	ap_power_ev_add_callback(&cb);
}

static int intel_altmode_init(const struct device *dev)
{
	const struct pd_altmode_config *cfg = dev->config;
	struct pd_altmode_data *data = dev->data;
	int rv;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR("I2C is not ready");
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->int_gpio)) {
		LOG_ERR("GPIO is not ready");
		return -ENODEV;
	}

	data->dev = dev;

	/* Configure interrupt for the primary port in case of a dual port PD */
	if (!cfg->shared_irq) {
		rv = gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
		if (rv < 0) {
			LOG_ERR("Unable to configure GPIO");
			return rv;
		}

		gpio_init_callback(&data->gpio_cb, pd_altmode_gpio_callback,
				   BIT(cfg->int_gpio.pin));

		k_work_init(&data->work, pd_altmode_isr_work);

		rv = gpio_add_callback(cfg->int_gpio.port, &data->gpio_cb);
		if (rv < 0) {
			LOG_ERR("Unable to add callback");
			return rv;
		}

		rv = gpio_pin_interrupt_configure_dt(&cfg->int_gpio,
						     GPIO_INT_EDGE_FALLING);
		if (rv < 0) {
			LOG_ERR("Unable to configure interrupt");
			return rv;
		}
	}

	static bool init = false;
	if (init) {
		return 0;
	}
	/* This needs to be only once, currently only testing with Port 0 */
	intel_altmode_init_cbs();

	init_drp();

	init = true;
	return 0;
}

#define INTEL_ALTMODE_DEFINE(inst)                                        \
	static struct pd_altmode_data pd_altmode_data_##inst;             \
                                                                          \
	static const struct pd_altmode_config pd_altmode_config##inst = { \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
		.shared_irq = DT_INST_PROP(inst, irq_shared),             \
	};                                                                \
                                                                          \
	DEVICE_DT_INST_DEFINE(inst, intel_altmode_init, NULL,             \
			      &pd_altmode_data_##inst,                    \
			      &pd_altmode_config##inst, POST_KERNEL,      \
			      CONFIG_APPLICATION_INIT_PRIORITY,           \
			      &intel_pd_altmode_driver_api);

DT_INST_FOREACH_STATUS_OKAY(INTEL_ALTMODE_DEFINE)
