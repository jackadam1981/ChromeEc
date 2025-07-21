/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "gpio.h"
#include "gpio/gpio_int.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(board_init, LOG_LEVEL_INF);
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

int board_discharge_on_ac(int enable)
{
	LOG_INF("Kaladin: discharge on AC: %d", enable);

	int port;

	if (enable) {
		port = charge_manager_get_active_charge_port();
		if (port == 0) {
			LOG_INF("discharge on AC port: %d", port);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable),
				1);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable),
				0);
		} else if (port == 1) {
			LOG_INF("discharge on AC port: %d", port);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable),
				0);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable),
				1);
		} else {
			LOG_INF("Unknown charge port");
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable),
				0);
			gpio_pin_set_dt(
				GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable),
				0);
		}
	} else {
		LOG_INF("Disable discharge on AC");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable),
				0);
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable),
				0);
	}

	return EC_SUCCESS;
}
