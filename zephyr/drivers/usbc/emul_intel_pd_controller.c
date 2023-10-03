/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Intel PD Controller Emulator responds to I2C transactions for registers
 * access. Register details can be found in respective SoC's "Platform Power
 * Delivery Controller Interface for SoC and Retimer" document.
 */

#include "emul/emul_stub_device.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/emul.h>
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

#define EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG   BIT(0)

#define EMUL_INTEL_PD_CONTROLLER_I2C_ADD(emul)                                 \
	(((const struct intel_pd_controller_emul_config*)emul->cfg)->i2c.addr)

#define EMUL_INTEL_PD_CONTROLLER_ARRAY_WITH_COMMA(node_id)                     \
					    EMUL_DT_GET(node_id),

const struct emul* pd_ctrlrs[] = {
	DT_FOREACH_STATUS_OKAY(intel_pd_altmode,
			EMUL_INTEL_PD_CONTROLLER_ARRAY_WITH_COMMA)
};

struct intel_pd_controller_emul_config {
	/* I2C config */
	struct i2c_dt_spec i2c;
	/* Interrupt GPIO */
	struct gpio_dt_spec int_gpio;
};

struct intel_pd_controller_emul_data {
	/* Emulated status register */
	union data_status_reg status;
	/* Internal emulator flags */
	uint8_t flags;
};

static void intel_pd_controller_emul_set_flags(const struct emul *emul,
					       const uint8_t flags)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	data->flags |= flags;
}

static uint8_t intel_pd_controller_emul_get_flags(const struct emul *emul)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	return data->flags;
}

static void intel_pd_controller_emul_clr_flags(const struct emul *emul,
					       const uint8_t flags)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	data->flags &= ~flags;
}


static void intel_pd_controller_emul_get_status(const struct emul *emul,
					        uint8_t *status)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	memcpy(status, &data->status, INTEL_ALTMODE_DATA_STATUS_REG_LEN);
}

static void intel_pd_controller_emul_set_irq(const struct emul *emul)
{
	const struct intel_pd_controller_emul_config *cfg = emul->cfg;

	gpio_emul_input_set(cfg->int_gpio.port, cfg->int_gpio.pin,
			EMUL_INTEL_PD_CONTROLLER_IRQ_SET);
}

static void intel_pd_controller_emul_clr_irq(const struct emul *emul)
{
	const struct intel_pd_controller_emul_config *cfg = emul->cfg;

	intel_pd_controller_emul_clr_flags(emul,
				EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG);

	/**
	 * If GPIO IRQ is shared with another device, check if all associated
	 * interruprions have been acknowledged before deasserting GPIO IRQ.
	 */
	for (int i = 0; i < ARRAY_SIZE(pd_ctrlrs); i++) {
		const struct intel_pd_controller_emul_config *_cfg;
		struct intel_pd_controller_emul_data *_data;

		if (pd_ctrlrs[i] == emul) {
			continue;
		}

		_cfg = pd_ctrlrs[i]->cfg;
		if (cfg->int_gpio.port != cfg->int_gpio.port ||
		    cfg->int_gpio.pin != cfg->int_gpio.pin) {
			continue;
		}

		_data = pd_ctrlrs[i]->data;
		if (_data->flags & EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG) {
			return;
		}
	}
	gpio_emul_input_set(cfg->int_gpio.port, cfg->int_gpio.pin,
			EMUL_INTEL_PD_CONTROLLER_IRQ_CLR);
}

static int intel_pd_controller_emul_transfer(const struct emul *emul,
				             struct i2c_msg *msgs, int num_msgs,
				             int addr)
{
	union data_control_reg ctrl_reg;
	uint8_t flags;

	if (EMUL_INTEL_PD_CONTROLLER_I2C_ADD(emul) != addr) {
		return -ENODEV;
	}

	if (num_msgs == 1) {
		if (!(((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == INTEL_ALTMODE_DATA_CONTROL_REG_LEN + 2))) {
			return -EINVAL;
		}
		if (msgs[0].buf[0] != INTEL_ALTMODE_REG_DATA_CONTROL ||
		    msgs[0].buf[1] != INTEL_ALTMODE_DATA_CONTROL_REG_LEN) {
			return -EINVAL;
		}

		memcpy(&ctrl_reg, &msgs[0].buf[2],
			INTEL_ALTMODE_DATA_CONTROL_REG_LEN);
		flags = intel_pd_controller_emul_get_flags(emul);
		if ((flags & EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG) &&
		    ctrl_reg.i2c_int_ack) {
			intel_pd_controller_emul_clr_irq(emul);
		}
	} else if (num_msgs == 2) {
		if (!(((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		    (msgs[0].len == 1) &&
		    (msgs[0].buf[2] != INTEL_ALTMODE_REG_DATA_STATUS) &&
		    ((msgs[1].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) &&
		    (msgs[1].len == INTEL_ALTMODE_DATA_STATUS_REG_LEN + 1))) {
			return -EINVAL;
		}
		msgs[1].buf[0] = INTEL_ALTMODE_DATA_STATUS_REG_LEN;
		intel_pd_controller_emul_get_status(emul, &msgs[1].buf[1]);
	} else {
		return -EINVAL;
	}
	return 0;
}

static struct i2c_emul_api intel_pd_controller_emul_bus_api = {
	.transfer = intel_pd_controller_emul_transfer,
};

static int intel_pd_controller_emul_init(const struct emul *emul,
					 const struct device *parent)
{
	const struct intel_pd_controller_emul_config *cfg = emul->cfg;

	gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	intel_pd_controller_emul_clr_irq(emul);

	return 0;
}

#define INTEL_PD_CONTROLLER_EMUL_DEFINE(inst)                             \
	static struct intel_pd_controller_emul_data                       \
		intel_pd_controller_emul_data_##inst;                     \
                                                                          \
	static const struct intel_pd_controller_emul_config               \
		intel_pd_controller_emul_config_##inst = {                \
		.i2c = I2C_DT_SPEC_INST_GET(inst),                        \
		.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios),       \
	};                                                                \
	EMUL_DT_INST_DEFINE(inst, intel_pd_controller_emul_init,          \
			    &intel_pd_controller_emul_data_##inst,        \
			    &intel_pd_controller_emul_config_##inst,      \
			    &intel_pd_controller_emul_bus_api, NULL)

DT_INST_FOREACH_STATUS_OKAY(INTEL_PD_CONTROLLER_EMUL_DEFINE)

/* Exported functions to manipulate emulator functionality */
int intel_pd_controller_emul_set_status(const struct emul *emul,
					const union data_status_reg status)
{
	struct intel_pd_controller_emul_data *data = emul->data;

	memcpy(&data->status, &status, INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	return 0;
}

int intel_pd_controller_emul_trigger_irq(const struct emul *emul)
{
	intel_pd_controller_emul_set_flags(emul,
					EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG);

	intel_pd_controller_emul_set_irq(emul);

	return 0;
}
