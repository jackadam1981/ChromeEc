/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

static const struct gpio_dt_spec interrupt_spec =
	GPIO_DT_SPEC_GET(DT_CHOSEN(cros_notebook_interrupt), gpios);

static int init_notebook_mode_interrupt() {
	return 0;
}

SYS_INIT(init_notebook_mode_interrupt, APPLICATION, 99);
