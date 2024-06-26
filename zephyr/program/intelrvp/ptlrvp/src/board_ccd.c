/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"

#include <zephyr/init.h>

LOG_MODULE_REGISTER(board_ccd, LOG_LEVEL_INF);

static int board_ccd_cntrl(void)
{
	const struct gpio_dt_spec *spec = GPIO_DT_FROM_NODELABEL(ccd_mode_odl);

	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(ccd_cntl))) {
		LOG_INF("C0: Debug port is for CCD");
		gpio_pin_configure(spec->port, spec->pin, GPIO_INPUT);
	} else  {
		LOG_INF("C0: Debug port is for Intel");
		gpio_pin_configure(spec->port, spec->pin, GPIO_OUTPUT_HIGH);
	}

	return 0;
}
SYS_INIT(board_ccd_cntrl, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
