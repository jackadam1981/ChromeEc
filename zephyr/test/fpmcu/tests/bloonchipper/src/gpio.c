/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio/gpio.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(gpio, NULL, NULL, NULL, NULL, NULL);

ZTEST(gpio, test_unused_gpio)
{
	const struct device *gpio0 = DEVICE_DT_GET(DT_NODELABEL(gpio0));
	gpio_flags_t flags;
	int ret;

	ret = gpio_config_unused_pins();
	zassert_equal(ret, -ENOTSUP, "Lack of GPIO device not signaled");

	ret = gpio_pin_get_config(gpio0, 4, &flags);
	zassert_equal(ret, 0, "Unable to get GPIO config");

	printk("DN: flags: 0x%x\n", flags);
	printk("DN: dts  : 0x%x\n", (GPIO_OUTPUT | GPIO_PULL_UP));
	zassert_equal(flags, (GPIO_OUTPUT_LOW | GPIO_PULL_UP),
		      "Incorrect flags passed via DTS");
}
