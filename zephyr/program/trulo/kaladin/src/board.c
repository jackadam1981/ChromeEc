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

int board_discharge_on_ac(int enable)
{
	LOG_INF("Kaladin: discharge on AC: %d", enable);

	int port, c0_hv_disable, c1_hv_disable;

	port = charge_manager_get_active_charge_port();
	c0_hv_disable = (enable && port == 0);
	c1_hv_disable = (enable && port == 1);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable),
			c0_hv_disable);
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable),
			c1_hv_disable);
	LOG_INF("Enable: %d, port: %d", enable, port);
	LOG_INF("gpio_usb_c0_hv_disable: %d",
		gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_usb_c0_hv_disable)));
	LOG_INF("gpio_usb_c1_hv_disable: %d",
		gpio_pin_get_dt(
			GPIO_DT_FROM_NODELABEL(gpio_usb_c1_hv_disable)));

	return EC_SUCCESS;
}
