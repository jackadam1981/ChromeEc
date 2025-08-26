/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "backlight.h"
#include "cros_cbi.h"
#include "gpio/gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "lid_switch.h"

#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(kinmen_touch, LOG_LEVEL_INF);

/* touch panel power sequence control */

#define TOUCH_ENABLE_DELAY_MS (500 * USEC_PER_MSEC)

void soc_edp_bl_interrupt(enum gpio_signal signal)
{
	int state;

	if (signal != GPIO_SIGNAL(DT_NODELABEL(gpio_soc_edp_bl_en)))
		return;

	state = gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_soc_edp_bl_en));

	enable_backlight(lid_is_open() && state);

	LOG_INF("%s: %d", __func__, state);
}
