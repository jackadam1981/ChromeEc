/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/tcpci.h"
#include "emul/gpio_controller_mock.h"
#include "tcpm/tcpm.h"
#include "test/drivers/stubs.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

/* Function signature for shim/src/gpio.c test_export_static */
int init_gpios(const struct device *unused);

ZTEST_SUITE(gpio_init, drivers_predicate_post_main, NULL, NULL, NULL, NULL);

ZTEST(gpio_init, test_gpio_not_ready)
{
	const struct gpio_dt_spec *gpio_not_ready_dev =
		GPIO_DT_FROM_NODELABEL(gpio_not_ready);

	zassert_equal(gpio_mock_controller_pin_configure_call_count(
			      gpio_not_ready_dev->port),
		      0);

	/* Validate the emulator catches calls the pin configure */
	gpio_pin_configure_dt(gpio_not_ready_dev, GPIO_INPUT);

	zassert_equal(gpio_mock_controller_pin_configure_call_count(
			      gpio_not_ready_dev->port),
		      1);
}
