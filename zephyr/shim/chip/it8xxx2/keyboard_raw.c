/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Functions needed by keyboard scanner module for Chrome EC */

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_kscan_it8xxx2.h>
#include <zephyr/logging/log.h>
#include <soc.h>
#include <zephyr/zephyr.h>

#include "drivers/cros_kb_raw.h"
#include "keyboard_raw.h"

/**
 * Return true if the current value of the given gpioksi/gpioksoh/gpioksol
 * port is zero
 */
int keyboard_raw_is_input_low(int port, int id)
{
	const struct device *io_dev = it8xxx2_get_gpio_kscan_dev(port);

	return gpio_pin_get_raw(io_dev, id) == 0;
}
