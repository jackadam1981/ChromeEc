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
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/i2c_emul.h>
#include <emul_intel_pd_controller.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#define DT_DRV_COMPAT intel_pd_altmode

/* Set level irq trigger accordingly */
#define EMUL_INTEL_PD_CONTROLLER_IRQ_SET    (0)
#define EMUL_INTEL_PD_CONTROLLER_IRQ_CLR    (!EMUL_INTEL_PD_CONTROLLER_IRQ_SET)

LOG_MODULE_DECLARE(INTEL_ALTMODE, LOG_LEVEL_ERR);

struct intel_pd_controller_emul_config {
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

struct intel_pd_controller_emul_data {
	const struct device *dev;
	union data_status_reg status;
	union data_control_reg control;
};

static void intel_pd_controller_emul_set_irq(const struct emul *emul,
					     const bool value)
{
	const struct intel_pd_controller_emul_config *cfg = emul->cfg;

	if (!cfg->shared_irq) {
		gpio_emul_input_set(cfg->int_gpio.port, cfg->int_gpio.pin,
				    value);
	}
}

static int intel_pd_controller_emul_transfer(const struct emul *emul,
				             struct i2c_msg *msgs, int num_msgs,
				             int addr)
{
	const struct intel_pd_controller_emul_config *cfg = emul->cfg;
	struct intel_pd_controller_emul_data *data = emul->data;

	if (cfg->i2c.addr != addr) {
		return -1;
	}

	if (num_msgs == 1) {
		if (!(((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == INTEL_ALTMODE_DATA_CONTROL_REG_LEN + 2))) {
			return -1;
		}
		if (msgs[0].buf[0] != INTEL_ALTMODE_REG_DATA_CONTROL ||
		    msgs[0].buf[1] != INTEL_ALTMODE_DATA_CONTROL_REG_LEN) {
			return -1;
		}
		memcpy(&data->control, &msgs[0].buf[2],
			INTEL_ALTMODE_DATA_CONTROL_REG_LEN);
		if (data->control.i2c_int_ack) {
			intel_pd_controller_emul_set_irq(emul,
					EMUL_INTEL_PD_CONTROLLER_IRQ_CLR);
		}
	} else if (num_msgs == 2) {
		if (!(((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == 1) &&
		    (msgs[0].buf[2] != INTEL_ALTMODE_REG_DATA_STATUS) &&
		    ((msgs[1].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) &&
		    (msgs[1].len == INTEL_ALTMODE_DATA_STATUS_REG_LEN + 1))) {
			return -1;
		}
		memcpy(&msgs[1].buf[2], &data->status,
			INTEL_ALTMODE_DATA_STATUS_REG_LEN);
	} else {
		return -1;
	}
	return 0;
}

static struct i2c_emul_api intel_pd_controller_emul_bus_api = {
	.transfer = intel_pd_controller_emul_transfer,
};

static int intel_pd_controller_emul_init(const struct emul *emul,
					 const struct device *parent)
{
	intel_pd_controller_emul_set_irq(emul,
					 EMUL_INTEL_PD_CONTROLLER_IRQ_CLR);

	return 0;
}

#define INTEL_ALTMODE_DEFINE(inst)                                        \
	static struct intel_pd_controller_emul_data                       \
		intel_pd_controller_emul_data_##inst;                     \
                                                                          \
	static const struct intel_pd_controller_emul_config               \
		intel_pd_controller_emul_config_##inst = {                \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
		.shared_irq = DT_INST_PROP(inst, irq_shared),             \
	};                                                                \
	EMUL_DT_INST_DEFINE(inst, intel_pd_controller_emul_init,          \
			    &intel_pd_controller_emul_data_##inst,        \
			    &intel_pd_controller_emul_config_##inst,      \
			    &intel_pd_controller_emul_bus_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(INTEL_ALTMODE_DEFINE)

int intel_pd_controller_emul_set_status(const struct emul *emul,
					const union data_status_reg status)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	memcpy(&data->status, &status, INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	return 0;
}

int intel_pd_controller_emul_trigger_irq(const struct emul *emul)
{
	intel_pd_controller_emul_set_irq(emul,
					 EMUL_INTEL_PD_CONTROLLER_IRQ_SET);

	return 0;
}
