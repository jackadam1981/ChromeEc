/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "keyboard_scan.h"
#include "system.h"

#include <zephyr/device.h>
#include <zephyr/fff.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(host_set_single_event, enum host_event_code);
FAKE_VALUE_FUNC(int, system_jumped_late);
FAKE_VALUE_FUNC(uint32_t, system_get_reset_flags);
FAKE_VALUE_FUNC(int, power_button_is_pressed);

void test_power_button_change(void);
int test_reinit(void);
bool test_dwork_pending(void);
uint32_t keyboard_scan_get_boot_keys(void);

#define CROS_EC_KEYBOARD_NODE DT_CHOSEN(cros_ec_keyboard)
#define DELAY_NO_TRIGGER DT_PROP(CROS_EC_KEYBOARD_NODE, debounce_down_ms)
#define DELAY_TRIGGER (DT_PROP(CROS_EC_KEYBOARD_NODE, debounce_down_ms) * 4)

static void report_fake(int row, int col, bool val)
{
	const struct device *dev = DEVICE_DT_GET(CROS_EC_KEYBOARD_NODE);
	input_report_abs(dev, INPUT_ABS_X, col, false, K_FOREVER);
	input_report_abs(dev, INPUT_ABS_Y, row, false, K_FOREVER);
	input_report_key(dev, INPUT_BTN_TOUCH, val, true, K_FOREVER);
}

ZTEST(boot_keys, test_recovery_normal)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	k_sleep(K_MSEC(DELAY_NO_TRIGGER));
	zassert_equal(test_dwork_pending(), true);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);

	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);
	zassert_equal(host_set_single_event_fake.call_count, 1);
	zassert_equal(keyboard_scan_get_boot_keys(), 0x80000009);
}

ZTEST(boot_keys, test_recovery_release_power_early)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	k_sleep(K_MSEC(DELAY_NO_TRIGGER));
	zassert_equal(test_dwork_pending(), true);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);

	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);
	power_button_is_pressed_fake.return_val = 0;
	test_power_button_change();

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);
	zassert_equal(host_set_single_event_fake.call_count, 1);
	zassert_equal(keyboard_scan_get_boot_keys(), 0x00000009);
}

ZTEST(boot_keys, test_recovery_stray)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	k_sleep(K_MSEC(DELAY_NO_TRIGGER));
	zassert_equal(test_dwork_pending(), true);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);

	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);
	/* strays */
	report_fake(10, 11, true);
	report_fake(12, 13, true);
	report_fake(10, 11, false);

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);
}

ZTEST(boot_keys, test_recovery_retraining)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);
	report_fake(4, 5, true);

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);
	zassert_equal(host_set_single_event_fake.call_count, 2);
	zassert_equal(keyboard_scan_get_boot_keys(), 0x8000000d);
}

ZTEST(boot_keys, test_ignore_keys)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	k_sleep(K_MSEC(DELAY_NO_TRIGGER));
	zassert_equal(test_dwork_pending(), true);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);

	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);

	/* ignored stray keys */
	report_fake(2, 10, true);
	report_fake(2, 11, true);
	report_fake(2, 12, true);

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);

	if (IS_ENABLED(CONFIG_BOOT_KEYS_COL2_WORKAROUND)) {
		zassert_equal(host_set_single_event_fake.call_count, 1);
		zassert_equal(keyboard_scan_get_boot_keys(), 0x80000009);
	} else {
		zassert_equal(host_set_single_event_fake.call_count, 0);
		zassert_equal(keyboard_scan_get_boot_keys(), 0);
	}
}

ZTEST(boot_keys, test_normal_boot)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.return_val = EC_RESET_FLAG_RESET_PIN;

	test_reinit();

	k_sleep(K_MSEC(DELAY_NO_TRIGGER));
	zassert_equal(test_dwork_pending(), true);

	k_sleep(K_MSEC(DELAY_TRIGGER));
	zassert_equal(test_dwork_pending(), false);
	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);

	/* no change after the timeout */
	power_button_is_pressed_fake.return_val = 1;
	test_power_button_change();
	report_fake(2, 3, true);
	report_fake(6, 7, true);

	zassert_equal(host_set_single_event_fake.call_count, 0);
	zassert_equal(keyboard_scan_get_boot_keys(), 0);
}

ZTEST(boot_keys, test_no_reset_pin)
{
	system_jumped_late_fake.return_val = 0;
	system_get_reset_flags_fake.call_count = 0;

	test_reinit();

	zassert_equal(test_dwork_pending(), false);
}

ZTEST(boot_keys, test_jumped_late)
{
	system_jumped_late_fake.return_val = 1;

	test_reinit();

	zassert_equal(system_get_reset_flags_fake.call_count, 0);
	zassert_equal(test_dwork_pending(), false);
}

static void reset(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(host_set_single_event);
	RESET_FAKE(system_jumped_late);
	RESET_FAKE(system_get_reset_flags);
	RESET_FAKE(power_button_is_pressed);
}

ZTEST_SUITE(boot_keys, NULL, NULL, reset, reset, NULL);
