/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "cros_button.h"
#include "drivers/cros_button.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"

LOG_MODULE_REGISTER(button_cfg, LOG_LEVEL_ERR);

#define DT_DRV_COMPAT cros_ec_button

static int button_config_init(const struct device *dev)
{
	return 0;
}

static const struct cros_button_api button_driver_api = {
	.get_config = cros_button_get_cfg,
	.get_debounce_us = cros_button_get_debounce_us,
	.enable_interrupt = cros_button_enable_interrupt,
	.disable_interrupt = cros_button_disable_interrupt,
	.is_pressed = cros_button_is_pressed,
	.is_pressed_raw = cros_button_is_pressed_raw,
};

#define BUTTON_CONFIG_INIT(i)                                                 \
	static const struct cros_button_config button_config_##i =            \
		BUTTON_CFG_DEF(i);                                            \
	static struct cros_button_data button_data_##i;                       \
	DEVICE_DT_INST_DEFINE(i, &button_config_init, NULL, &button_data_##i, \
			      &button_config_##i, POST_KERNEL,                \
			      CONFIG_PLATFORM_EC_BUTTON_INIT_PRIORITY,        \
			      &button_driver_api);

DT_INST_FOREACH_STATUS_OKAY(BUTTON_CONFIG_INIT)
