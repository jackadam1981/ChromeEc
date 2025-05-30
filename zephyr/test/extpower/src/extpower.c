/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "extpower.h"
#include "hooks.h"
#include "host_command.h"
#include "lpc.h"

#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/ztest.h>

static int ac_hook_count;
static const struct device *acok_gpio_dev =
	DEVICE_DT_GET(DT_GPIO_CTLR(DT_NODELABEL(gpio_acok_od), gpios));
static const gpio_port_pins_t acok_pin =
	DT_GPIO_PIN(DT_NODELABEL(gpio_acok_od), gpios);

static void test_ac_change_hook(void)
{
	ac_hook_count++;
}
DECLARE_HOOK(HOOK_AC_CHANGE, test_ac_change_hook, HOOK_PRIO_DEFAULT);

static void set_ac(int on, bool wait)
{
	gpio_emul_input_set(acok_gpio_dev, acok_pin, on);

	if (wait) {
		k_msleep(CONFIG_PLATFORM_EC_EXTPOWER_DEBOUNCE_MS * 10);
	}
}

ZTEST(extpower, test_extpower_gpio)
{
	set_ac(0, true);
	ac_hook_count = 0;

	host_clear_events(0xFFFFFFFF);

	set_ac(1, true);
	zassert_equal(ac_hook_count, 1);
	zassert_equal(extpower_is_present(), 1);

	zassert_true(host_is_event_set(EC_HOST_EVENT_AC_CONNECTED));

	set_ac(0, true);
	zassert_equal(ac_hook_count, 2);
	zassert_equal(extpower_is_present(), 0);

	zassert_true(host_is_event_set(EC_HOST_EVENT_AC_DISCONNECTED));
}

ZTEST(extpower, test_extpower_gpio_debounce)
{
	/* Verify that changes to AC OK that are less than the debounce time
	 * do not generate HOOK or HOSTCMD events.
	 */
	set_ac(0, true);
	ac_hook_count = 0;

	host_clear_events(0xFFFFFFFF);

	set_ac(1, false);
	k_msleep(CONFIG_PLATFORM_EC_EXTPOWER_DEBOUNCE_MS / 2);
	set_ac(0, true);

	zassert_equal(ac_hook_count, 0);

	zassert_false(host_is_event_set(EC_HOST_EVENT_AC_CONNECTED));
	zassert_false(host_is_event_set(EC_HOST_EVENT_AC_DISCONNECTED));
}

ZTEST_SUITE(extpower, NULL, NULL, NULL, NULL, NULL);
