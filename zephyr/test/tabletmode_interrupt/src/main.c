/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(tabletmode_interrupt, NULL, NULL, NULL, NULL, NULL);

ZTEST(tabletmode_interrupt, test_gpio_toggles_tablet_mode)
{
	const struct gpio_dt_spec spec =
		GPIO_DT_SPEC_GET(DT_NODELABEL(tabletmode_interrupt), irq_gpios);

	/* Set pin to low, wait for sys-work queue to process events, then check
	 * tablet mode
	 */
	zassert_ok(gpio_emul_input_set(spec.port, spec.pin, 0));
	k_msleep(1);
	zassert_true(tablet_get_mode() == 0,
		     "Expected not to be in tablet mode");

	/* Set pin to high, wait for sys-work queue to process events, then
	 * check tablet mode
	 */
	zassert_ok(gpio_emul_input_set(spec.port, spec.pin, 1));
	k_msleep(1);
	zassert_true(tablet_get_mode() != 0, "Expected to be in tablet mode");
}
