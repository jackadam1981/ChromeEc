/*
 * Copyright (c) 2025 ITE Corporation. All Rights Reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>
#include <ap_power/ap_power.h>
#include "console.h"

#define I2C5_TARGET DT_NODELABEL(i2c5_target)
#define I2C5_NODE DT_BUS(I2C5_TARGET)
PINCTRL_DT_DEFINE(I2C5_NODE);
const struct device *i2c_target = DEVICE_DT_GET(I2C5_TARGET);

static int command_pin_input_pulldown(int argc, const char **argv)
{
	const struct pinctrl_dev_config *pcfg = PINCTRL_DT_DEV_CONFIG_GET(I2C5_NODE);

	if (!strcmp(argv[1], "1")) {
		pinctrl_apply_state(pcfg, PINCTRL_STATE_DEFAULT);
	} else if (!strcmp(argv[1], "0")) {
		pinctrl_apply_state(pcfg, PINCTRL_STATE_SLEEP);
	}

	return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(pininput, command_pin_input_pulldown, NULL, "Setting pin input pulldown");

static void board_suspend_handler(struct ap_power_ev_callback *cb, struct ap_power_ev_data data)
{

	switch (data.event) {
	default:
		return;

	case AP_POWER_STARTUP:

		i2c_target_driver_register(i2c_target);
		break;

	case AP_POWER_HARD_OFF:

		i2c_target_driver_unregister(i2c_target);
		break;
	}
}

static int install_suspend_handler(void)
{
	static struct ap_power_ev_callback cb;

	/*
	 * Add a callback for suspend/resume.
	 */
	ap_power_ev_init_callback(&cb, board_suspend_handler, AP_POWER_RESUME | AP_POWER_SUSPEND);
	ap_power_ev_add_callback(&cb);

	/* Register I2C target */
	i2c_target_driver_register(i2c_target);

	return 0;
}

SYS_INIT(install_suspend_handler, APPLICATION, 1);
