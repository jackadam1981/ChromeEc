/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charge_state_v2.h"
#include "math_util.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/ztest.h>

int battery_outside_charging_temperature(void);

struct charge_state_v2_fixture {
	struct charge_state_data charge_state_data;
};

static void *setup(void)
{
	static struct charge_state_v2_fixture fixture;

	return &fixture;
}

static void before(void *f)
{
	struct charge_state_v2_fixture *fixture = f;

	fixture->charge_state_data = *charge_get_status();
}

static void after(void *f)
{
	struct charge_state_v2_fixture *fixture = f;

	*charge_get_status() = fixture->charge_state_data;
}

ZTEST_SUITE(charge_state_v2, drivers_predicate_post_main, setup, before, after,
	    NULL);

#if 0
ZTEST(charge_state_v2, test_battery_flag_bad_temperature)
{
	struct charge_state_data *curr = charge_get_status();

	curr->batt.flags |= BATT_FLAG_BAD_TEMPERATURE;
	zassert_ok(battery_outside_charging_temperature());
}

ZTEST(charge_state_v2, test_battery_temperature_range)
{
	struct charge_state_data *curr = charge_get_status();
	const struct battery_info *batt_info = battery_get_info();

	curr->batt.flags &= ~BATT_FLAG_BAD_TEMPERATURE;

	/* Start off without a desired voltage/current */
	curr->batt.desired_voltage = 0;
	curr->batt.desired_current = 0;

	/* Temperature is too high */
	curr->batt.temperature =
		CELSIUS_TO_DECI_KELVIN(batt_info->start_charging_max_c + 1);
	zassert_equal(1, battery_outside_charging_temperature());

	/* Temperature is too low */
	curr->batt.temperature =
		CELSIUS_TO_DECI_KELVIN(batt_info->start_charging_min_c - 1);
	zassert_equal(1, battery_outside_charging_temperature());

	/* Temperature is just right */
	curr->batt.temperature =
		CELSIUS_TO_DECI_KELVIN((batt_info->start_charging_max_c +
					batt_info->start_charging_min_c) /
				       2);
	zassert_ok(battery_outside_charging_temperature());

	/* Set an arbitrary desired current */
	curr->batt.desired_current = 3;

	/* Temperature is too high */
	curr->batt.temperature =
		CELSIUS_TO_DECI_KELVIN(batt_info->charging_max_c + 1);
	zassert_equal(1, battery_outside_charging_temperature());

	/* Set an arbitrary desired voltage */
	curr->batt.desired_voltage = 5;

	/* Temperature is too low */
	curr->batt.temperature =
		CELSIUS_TO_DECI_KELVIN(batt_info->charging_min_c - 1);
	zassert_equal(1, battery_outside_charging_temperature());

	/* Temperature is just right */
	curr->batt.temperature = CELSIUS_TO_DECI_KELVIN(
		(batt_info->charging_max_c + batt_info->charging_min_c) / 2);
	zassert_ok(battery_outside_charging_temperature());
}

ZTEST(charge_state_v2, test_current_limit_derating)
{
	int charger_current_limit;

	charge_set_input_current_limit(1000, 5000);
	zassert_ok(charger_get_input_current_limit(0, &charger_current_limit));
	/*
	 * ISL923x sets ICL in multiples of 20 mA, so 950 mA gets rounded down
	 * to the nearest multiple of 20.
	 */
	zassert_equal(
		charger_current_limit, 944,
		"%d%% derating of 1A should be 944 mA, but charger is set for %d mA",
		CONFIG_PLATFORM_EC_CHARGER_INPUT_CURRENT_DERATE_PCT,
		charger_current_limit);
}

ZTEST(charge_state_v2, test_minimum_current_limit)
{
	int charger_current_limit;

	charge_set_input_current_limit(50, 5000);
	zassert_ok(charger_get_input_current_limit(0, &charger_current_limit));
	zassert_equal(charger_current_limit, 96,
		      "Minimum input current limit should be %d mA,"
		      " but current limit is %d (capped to %d)",
		      96, charger_current_limit,
		      CONFIG_PLATFORM_EC_CHARGER_MIN_INPUT_CURRENT_LIMIT);
}
#endif
extern int charge_port;

void charge_manager_set_active_charge_port(int port)
{
	charge_port = port;
}


ZTEST(charge_state_v2, test_charge_get_state)
{
	struct charge_state_data *curr = charge_get_status();

	set_ac_enabled(true);

	set_charge_state(ST_IDLE);
	//PWR_STATE_ERROR -- 10
	curr->batt.is_present = BP_NO;
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_ERROR, charge_get_state(), NULL);

	//PWR_STATE_FORCED_IDLE -- 5
	curr->batt.is_present = BP_YES;
	set_chg_ctrl_mode(CHARGE_CONTROL_IDLE);
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_FORCED_IDLE, charge_get_state(), NULL);

	//PWR_STATE_IDLE -- 4
	set_chg_ctrl_mode(CHARGE_CONTROL_NORMAL);
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_IDLE, charge_get_state(), NULL);

	set_charge_state(ST_DISCHARGE);
	//PWR_STATE_DISCHARGE -- 6
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_DISCHARGE, charge_get_state(), NULL);

	set_charge_state(ST_CHARGE);
	charge_manager_set_active_charge_port(-1);
	//PWR_STATE_DISCHARGE -- 6
	printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_DISCHARGE, charge_get_state(), NULL);

	charge_manager_set_active_charge_port(0);
	curr->batt.state_of_charge = 100;
	//PWR_STATE_CHARGE_NEAR_FULL -- 9
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_CHARGE_NEAR_FULL, charge_get_state(), NULL);

	curr->batt.state_of_charge = 50;
	//PWR_STATE_CHARGE -- 8
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_CHARGE, charge_get_state(), NULL);

	set_charge_state(ST_PRECHARGE);
	//PWR_STATE_FORCED_IDLE -- 5
	set_chg_ctrl_mode(CHARGE_CONTROL_IDLE);
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_FORCED_IDLE, charge_get_state(), NULL);

	//PWR_STATE_IDLE -- 4
	set_chg_ctrl_mode(CHARGE_CONTROL_NORMAL);
	//printf("\n\n---charge_get_state = %d---\n\n",charge_get_state());
	zassert_equal(PWR_STATE_IDLE, charge_get_state(), NULL);

	//zassert_true(0);
}
