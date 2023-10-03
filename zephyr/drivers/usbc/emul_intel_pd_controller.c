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
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <drivers/intel_altmode.h>
#include <emul_intel_pd_controller.h>

#define DT_DRV_COMPAT intel_pd_altmode

/* Set level irq trigger accordingly */
#define EMUL_INTEL_PD_CONTROLLER_IRQ_SET (0)
#define EMUL_INTEL_PD_CONTROLLER_IRQ_CLR (!EMUL_INTEL_PD_CONTROLLER_IRQ_SET)

#define EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG BIT(0)

#define EMUL_INTEL_PD_CONTROLLER_I2C_ADD(target)                       \
	(((const struct intel_pd_controller_emul_config *)target->cfg) \
		 ->i2c.addr)

#define EMUL_INTEL_PD_CONTROLLER_ARRAY_WITH_COMMA(node_id) EMUL_DT_GET(node_id),

const struct emul *pd_ctrlrs[] = { DT_FOREACH_STATUS_OKAY(
	intel_pd_altmode, EMUL_INTEL_PD_CONTROLLER_ARRAY_WITH_COMMA) };

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

static uint8_t intel_pd_controller_emul_get_flags(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	return data->flags;
}

static void intel_pd_controller_emul_clr_flags(const struct emul *target,
					       const uint8_t flags)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->flags &= ~flags;
}

static void intel_pd_controller_emul_get_status(const struct emul *target,
						uint8_t *status)
{
	struct intel_pd_controller_emul_data *data = target->data;

	memcpy(status, &data->status, INTEL_ALTMODE_DATA_STATUS_REG_LEN);
}

static void intel_pd_controller_emul_set_irq(const struct emul *target)
{
	const struct intel_pd_controller_emul_config *cfg = target->cfg;

	gpio_emul_input_set(cfg->int_gpio.port, cfg->int_gpio.pin,
			    EMUL_INTEL_PD_CONTROLLER_IRQ_SET);
}

static void intel_pd_controller_emul_clr_irq(const struct emul *target)
{
	const struct intel_pd_controller_emul_config *cfg = target->cfg;

	intel_pd_controller_emul_clr_flags(target,
					   EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG);

	/* Check it this IRQ is shared */
	for (int i = 0; i < ARRAY_SIZE(pd_ctrlrs); i++) {
		const struct intel_pd_controller_emul_config *_cfg;
		struct intel_pd_controller_emul_data *_data;

		if (pd_ctrlrs[i] == target) {
			/* Same emulator instance, skip this. */
			continue;
		}

		_cfg = pd_ctrlrs[i]->cfg;
		if (cfg->int_gpio.port != cfg->int_gpio.port ||
		    cfg->int_gpio.pin != cfg->int_gpio.pin) {
			/**
			 * Not sharing same GPIO port or pin with this
			 * emulator instance, continue.
			 **/
			continue;
		}

		_data = pd_ctrlrs[i]->data;
		if (_data->flags & EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG) {
			/**
			 * Sharing same IRQ GPIO with other emulator instance
			 * that has a pending interruption, deassert GPIO until
			 * all instances have interruption cleared.
			 **/
			return;
		}
	}
	gpio_emul_input_set(cfg->int_gpio.port, cfg->int_gpio.pin,
			    EMUL_INTEL_PD_CONTROLLER_IRQ_CLR);
}

static int intel_pd_controller_emul_transfer(const struct emul *target,
					     struct i2c_msg *msgs, int num_msgs,
					     int addr)
{
	union data_control_reg ctrl_reg;
	uint8_t flags;

	if (EMUL_INTEL_PD_CONTROLLER_I2C_ADD(target) != addr) {
		return -ENODEV;
	}

	if (num_msgs == 1) {
		/**
		 * Single message certainly means is write operation, check if
		 * message length matches with register being accessed.
		 **/
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
		flags = intel_pd_controller_emul_get_flags(target);
		if ((flags & EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG) &&
		    ctrl_reg.i2c_int_ack) {
			/* Clearing pending interruption */
			intel_pd_controller_emul_clr_irq(target);
		}
	} else if (num_msgs == 2) {
		/**
		 * This must be a read operation to access status register,
		 * check that valid register is requested and correct data
		 * length is provided.
		 **/
		if (!(((msgs[0].flags & I2C_MSG_RW_MASK) == I2C_MSG_WRITE) &&
		      (msgs[0].len == 1) &&
		      (msgs[0].buf[2] != INTEL_ALTMODE_REG_DATA_STATUS) &&
		      ((msgs[1].flags & I2C_MSG_RW_MASK) == I2C_MSG_READ) &&
		      (msgs[1].len == INTEL_ALTMODE_DATA_STATUS_REG_LEN + 1))) {
			return -EINVAL;
		}
		msgs[1].buf[0] = INTEL_ALTMODE_DATA_STATUS_REG_LEN;
		intel_pd_controller_emul_get_status(target, &msgs[1].buf[1]);
	} else {
		return -EINVAL;
	}
	return 0;
}

static struct i2c_emul_api intel_pd_controller_emul_bus_api = {
	.transfer = intel_pd_controller_emul_transfer,
};

static int intel_pd_controller_emul_init(const struct emul *target,
					 const struct device *parent)
{
	const struct intel_pd_controller_emul_config *cfg = target->cfg;

	gpio_pin_configure_dt(&cfg->int_gpio, GPIO_INPUT);
	intel_pd_controller_emul_clr_irq(target);

	return 0;
}

/* Exported functions to manipulate emulator functionality */
static int intel_pd_controller_emul_connect_data(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.data_conn = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

static int intel_pd_controller_emul_connect_usb2(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.usb2 = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

static int intel_pd_controller_emul_connect_usb3_2(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.usb3_2 = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

static int intel_pd_controller_emul_connect_dp(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.dp = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

static int intel_pd_controller_emul_set_dp_irq(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.dp_irq = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

static int intel_pd_controller_emul_set_hpd_lvl(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	data->status.hpd_lvl = 1;

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

struct emul_intel_pd_controller_backend_api
	intel_pd_controller_emul_backend_api = {
		.connect_data = intel_pd_controller_emul_connect_data,
		.connect_usb2 = intel_pd_controller_emul_connect_usb2,
		.connect_usb3_2 = intel_pd_controller_emul_connect_usb3_2,
		.connect_dp = intel_pd_controller_emul_connect_dp,
		.set_dp_irq = intel_pd_controller_emul_set_dp_irq,
		.set_hpd_lvl = intel_pd_controller_emul_set_hpd_lvl,
	};

static int intel_pd_controller_emul_reset(const struct emul *target)
{
	struct intel_pd_controller_emul_data *data = target->data;

	memset(&data->status, 0, INTEL_ALTMODE_DATA_STATUS_REG_LEN);

	data->flags |= EMUL_INTEL_PD_CONTROLLER_IRQ_FLAG;

	intel_pd_controller_emul_set_irq(target);

	return 0;
}

#define INTEL_PD_CONTROLLER_EMUL_RESET_RULE_BEFORE(inst)                                                    \
	intel_pd_controller_emul_reset(&EMUL_DT_NAME_GET(DT_DRV_INST(inst)));

static void intel_pd_controller_reset_before(const struct ztest_unit_test *test, void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	DT_INST_FOREACH_STATUS_OKAY(INTEL_PD_CONTROLLER_EMUL_RESET_RULE_BEFORE)
}
ZTEST_RULE(usbc_intel_altmode, intel_pd_controller_reset_before, NULL);

#define INTEL_PD_CONTROLLER_EMUL_DEFINE(inst)                               \
	static struct intel_pd_controller_emul_data                         \
		intel_pd_controller_emul_data_##inst;                       \
                                                                            \
	static const struct intel_pd_controller_emul_config                 \
		intel_pd_controller_emul_config_##inst = {                  \
			.i2c = I2C_DT_SPEC_INST_GET(inst),                  \
			.int_gpio = GPIO_DT_SPEC_INST_GET(inst, irq_gpios), \
		};                                                          \
                                                                            \
	EMUL_DT_INST_DEFINE(inst, intel_pd_controller_emul_init,            \
			    &intel_pd_controller_emul_data_##inst,          \
			    &intel_pd_controller_emul_config_##inst,        \
			    &intel_pd_controller_emul_bus_api,              \
			    &intel_pd_controller_emul_backend_api)

DT_INST_FOREACH_STATUS_OKAY(INTEL_PD_CONTROLLER_EMUL_DEFINE)
