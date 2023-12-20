/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include "battery_fuel_gauge.h"
#include "board_config.h"
// #include "button.h"
#include "charge_manager.h"
#include "chipset.h"
#include "common.h"
#include "craaskov.h"
// #include "cros_board_info.h"
// #include "cros_cbi.h"
#include "emul/tcpc/emul_tcpci.h"
#include "extpower.h"
// #include "fan.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
// #include "keyboard_8042_sharedlib.h"
// #include "keyboard_raw.h"
// #include "keyboard_scan.h"
// #include "led_onoff_states.h"
// #include "led_pwm.h"
// #include "motionsense_sensors.h"
// #include "nissa_sub_board.h"
// #include "tablet_mode.h"
#include "system.h"
#include "tcpm/tcpci.h"
// #include "thermal.h"


#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>

#include <dt-bindings/gpio_defines.h>
// #include <typec_control.h>

// #define TCPC0 EMUL_DT_GET(DT_NODELABEL(tcpc_port0))
// #define TCPC1 EMUL_DT_GET(DT_NODELABEL(tcpc_port1))

LOG_MODULE_REGISTER(nissa, LOG_LEVEL_INF);

FAKE_VALUE_FUNC(int, chipset_in_state, int);

FAKE_VALUE_FUNC(enum ec_error_list, raa489000_is_acok, int, bool *);
FAKE_VALUE_FUNC(enum battery_present, battery_is_present);
// FAKE_VOID_FUNC(set_pwm_led_color, enum pwm_led_id, int);
FAKE_VOID_FUNC(raa489000_hibernate, int, bool);

FAKE_VALUE_FUNC(int, raa489000_enable_asgate, int, bool);
FAKE_VALUE_FUNC(int, raa489000_set_output_current, int, enum tcpc_rp_value);

static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok)
{
	*acok = false;
	return EC_SUCCESS;
}

static enum ec_error_list raa489000_is_acok_present(int charger, bool *acok)
{
	*acok = true;
	return EC_SUCCESS;
}

static enum ec_error_list raa489000_is_acok_error(int charger, bool *acok)
{
	return EC_ERROR_UNIMPLEMENTED;
}
static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok);

// static enum ec_error_list raa489000_is_acok_absent(int charger, bool *acok);
static void test_before(void *fixture)
{
	RESET_FAKE(raa489000_hibernate);
	RESET_FAKE(raa489000_is_acok);
	raa489000_is_acok_fake.custom_fake = raa489000_is_acok_absent;

}
ZTEST_SUITE(craaskov, NULL, NULL, test_before, NULL, NULL);


ZTEST(craaskov, test_extpower_is_present)
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

static int extpower_handle_update_call_count;

static void call_extpower_handle_update(void)
{
	extpower_handle_update_call_count++;
}
DECLARE_HOOK(HOOK_AC_CHANGE, call_extpower_handle_update, HOOK_PRIO_DEFAULT);

ZTEST(craaskov, test_board_check_extpower)
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

ZTEST(craaskov, test_charger_hibernate)
{
	/* board_hibernate() asks the chargers to hibernate. */
	board_hibernate();

	zassert_equal(raa489000_hibernate_fake.call_count, 1);

	zassert_true(raa489000_hibernate_fake.arg1_history[0]);

}
