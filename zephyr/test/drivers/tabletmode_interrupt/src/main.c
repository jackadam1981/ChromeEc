/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "keyboard_scan.h"
#include "tablet_mode.h"
#include "tabletmode_interrupt/emul.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

struct tabletmode_interrupt_fixture {
	const struct gpio_dt_spec spec;
};

static void gpio_emul_dt_input_toggle(const struct gpio_dt_spec *spec,
				      int value)
{
	zassert_ok(gpio_emul_input_set(spec->port, spec->pin, !value));
	k_msleep(1);
	zassert_ok(gpio_emul_input_set(spec->port, spec->pin, value));
	k_msleep(1);
}

static void *tabletmode_interrupt_setup(void)
{
	static struct tabletmode_interrupt_fixture fixture = {
		.spec = GPIO_DT_SPEC_GET(DT_NODELABEL(tabletmode_interrupt),
					 irq_gpios),
	};

	return &fixture;
}

static void tabletmode_interrupt_before(void *f)
{
	ARG_UNUSED(f);

	tablet_reset();
	tabletmode_interrupt_set_device_ready(true);
}

static void tabletmode_interrupt_after(void *f)
{
	struct tabletmode_interrupt_fixture *fixture = f;

	gpio_emul_dt_input_toggle(&fixture->spec, 1);
}

ZTEST_SUITE(tabletmode_interrupt, NULL, tabletmode_interrupt_setup,
	    tabletmode_interrupt_before, tabletmode_interrupt_after, NULL);

ZTEST_F(tabletmode_interrupt, test_gpio_toggles_tablet_mode)
{
	/* Set pin to low, wait for sys-work queue to process events, then check
	 * tablet mode
	 */
	gpio_emul_dt_input_toggle(&fixture->spec, 0);
	zassert_true(tablet_get_mode() != 0, "Expected to be in tablet mode");

	/* Set pin to high, wait for sys-work queue to process events, then
	 * check tablet mode
	 */
	gpio_emul_dt_input_toggle(&fixture->spec, 1);
	zassert_true(tablet_get_mode() == 0,
		     "Expected not to be in tablet mode");
}

ZTEST(tabletmode_interrupt, test_bus_not_ready)
{
	tabletmode_interrupt_set_device_ready(false);

	zassert_equal(-EINVAL, tabletmode_init_mode_interrupt());
}

ZTEST_F(tabletmode_interrupt, test_suspend_enable_keyboard_scan)
{
	/* Set pin to low, wait for sys-work queue to process events, then check
	 * tablet mode
	 */
	gpio_emul_dt_input_toggle(&fixture->spec, 0);

	tabletmode_suspend_peripherals();
	zassert_false(keyboard_scan_is_enabled());
}
