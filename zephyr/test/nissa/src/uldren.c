/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "button.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "keyboard_protocol.h"
#include "led_onoff_states.h"
#include "led_pwm.h"
#include "pwm_mock.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

void form_factor_init(void);

FAKE_VALUE_FUNC(int, cros_cbi_get_fw_config, enum cbi_fw_config_field_id,
		uint32_t *);
FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
FAKE_VOID_FUNC(board_set_active_charge_port, int);
FAKE_VOID_FUNC(set_pwm_led_color, enum pwm_led_id, int);

int button_disable_gpio(enum button button_type)
{
	return EC_SUCCESS;
}

static void uldren_test_before(void *fixture)
{
	RESET_FAKE(cros_cbi_get_fw_config);
	RESET_FAKE(battery_is_present);
	RESET_FAKE(board_set_active_charge_port);
	RESET_FAKE(set_pwm_led_color);
}

ZTEST_SUITE(uldren, NULL, NULL, uldren_test_before, NULL, NULL);

ZTEST(craask, test_extpower_is_present)
{
	/* Errors are not-OK */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	zassert_false(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 2);

	/* When neither charger is connected, we check both and return no. */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_absent;
	zassert_false(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 4);

	/* If one is connected, AC is present */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	zassert_true(extpower_is_present());
	zassert_equal(raa489000_is_acok_fake.call_count, 5);
}

ZTEST(craask, test_board_check_extpower)
{
	/* Clear call count before testing. */
	extpower_handle_update_call_count = 0;

	/* Update with no change does nothing. */
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 0);

	/* Becoming present updates */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_present;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 1);

	/* Errors are treated as not plugged in */
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_error;
	board_check_extpower();
	zassert_equal(extpower_handle_update_call_count, 2);
}

ZTEST(craask, test_charger_hibernate)
{
	/* board_hibernate() asks the chargers to hibernate. */
	board_hibernate();

	zassert_equal(raa489000_hibernate_fake.call_count, 2);
	zassert_equal(raa489000_hibernate_fake.arg0_history[0],
		      CHARGER_SECONDARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[0]);
	zassert_equal(raa489000_hibernate_fake.arg0_history[1],
		      CHARGER_PRIMARY);
	zassert_true(raa489000_hibernate_fake.arg1_history[1]);
}
