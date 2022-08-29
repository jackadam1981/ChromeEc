/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include "drivers/cros_kb_raw.h"
#include "keyboard_raw.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <soc.h>

/**
 * Return true if the current value of the given gpioksi/gpioksoh/gpioksol
 * port is zero
 */
int keyboard_raw_is_input_low(int port, int id)
{
	const struct device *dev;

	if (port == 0) {
		dev = DEVICE_DT_GET(DT_NODELABEL(gpioksi));
	} else if (port == 1) {
		dev = DEVICE_DT_GET(DT_NODELABEL(gpioksoh));
	} else {
		dev = DEVICE_DT_GET(DT_NODELABEL(gpioksol));
	}

	return (gpio_pin_get_raw(dev, id) == 0);
}
