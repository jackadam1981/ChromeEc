/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "charge_state.h"
#include "power.h"

#include <zephyr/fff.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_test_new.h>

LOG_MODULE_REGISTER(led_policy);

#define DT_DRV_COMPAT cros_ec_led_policy

#define HAS_CROS_LED_POLICY DT_HAS_COMPAT_STATUS_OKAY(cros_ec_led_policy)

#define LED_POLICY_COUNTER(child_id) 1
#define LED_POLICY_COUNT                                           \
	DT_FOREACH_CHILD_SEP(                                      \
		DT_COMPAT_GET_ANY_STATUS_OKAY(cros_ec_led_policy), \
		LED_POLICY_COUNTER, (+))

struct dut_state {
	enum led_pwr_state pwr_state;
	int active_charge_port;
	int battery_status;
};

static struct dut_state
	led_policy_dut_states[LED_PWRS_COUNT - LED_PWRS_FORCED_IDLE];

FAKE_VALUE_FUNC(enum led_pwr_state, led_pwr_get_state);
FAKE_VALUE_FUNC(int, charge_manager_get_active_charge_port);
FAKE_VALUE_FUNC(int, battery_status, int *);
FAKE_VALUE_FUNC(int, charge_get_display_charge);
FAKE_VALUE_FUNC(int, chipset_in_state, int);

#define MOCK_LIST(FAKE)                                      \
	{                                                    \
		FAKE(led_pwr_get_state);                     \
		FAKE(charge_manager_get_active_charge_port); \
		FAKE(battery_status);                        \
		FAKE(charge_get_display_charge);             \
		FAKE(chipset_in_state);                      \
	}

static const char *led_pwrs_name[] = {
	[LED_PWRS_UNCHANGE] = "LED_PWRS_UNCHANGE",
	[LED_PWRS_INIT] = "LED_PWRS_INIT",
	[LED_PWRS_REINIT] = "LED_PWRS_REINIT",
	[LED_PWRS_IDLE0] = "LED_PWRS_IDLE0",
	[LED_PWRS_IDLE] = "LED_PWRS_IDLE",
	[LED_PWRS_FORCED_IDLE] = "LED_PWRS_FORCED_IDLE",
	[LED_PWRS_DISCHARGE] = "LED_PWRS_DISCHARGE",
	[LED_PWRS_DISCHARGE_FULL] = "LED_PWRS_DISCHARGE_FULL",
	[LED_PWRS_CHARGE] = "LED_PWRS_CHARGE",
	[LED_PWRS_CHARGE_NEAR_FULL] = "LED_PWRS_CHARGE_NEAR_FULL",
	[LED_PWRS_ERROR] = "LED_PWRS_ERROR",
	[LED_PWRS_COUNT] = "LED_PWRS_COUNT",
};

extern int match_node(int node_idx);

static int chipset_current_mask;
int chipset_in_state_custom_fake(int state_mask)
{
	return (state_mask & chipset_current_mask) == state_mask;
}

static void led_policy_rule_before(const struct ztest_unit_test *test,
				   void *data)
{
	ARG_UNUSED(test);
	ARG_UNUSED(data);

	MOCK_LIST(RESET_FAKE);

	FFF_RESET_HISTORY();

	chipset_in_state_fake.custom_fake = chipset_in_state_custom_fake;

	for (int i = 0; i < ARRAY_SIZE(led_policy_dut_states); i++) {
		led_policy_dut_states[i].pwr_state = LED_PWRS_FORCED_IDLE + i;
		led_policy_dut_states[i].active_charge_port = 0;
		led_policy_dut_states[i].battery_status = STATUS_CODE_OK;
	}
}

ZTEST_SUITE(led_policy, NULL, led_policy_rule_before, NULL, NULL, NULL);

ZTEST(led_policy, test_led_policy_self_test)
{
	zassert_equal(HAS_CROS_LED_POLICY, 1,
		      "cros-ec,led-policy self-test failed");
}

static int current_battery_status;
int battery_status_custom_fake(int *status)
{
	*status = current_battery_status;
	return EC_SUCCESS;
}

ZTEST(led_policy, test_led_policy_gaps_chipset_on)
{
	bool found_node = false;
	int batt_level, batt_min, batt_max;

	chipset_current_mask = CHIPSET_STATE_ON;

	for (int state = 0; state < ARRAY_SIZE(led_policy_dut_states);
	     state++) {
		struct dut_state *dut_state = &led_policy_dut_states[state];

		LOG_INF("Policy: %s, chg_port %d, batt_status 0x%08x",
			led_pwrs_name[dut_state->pwr_state],
			dut_state->active_charge_port,
			dut_state->battery_status);

		led_pwr_get_state_fake.return_val = dut_state->pwr_state;
		charge_manager_get_active_charge_port_fake.return_val =
			dut_state->active_charge_port;
		current_battery_status = dut_state->battery_status;

		if (dut_state->pwr_state == LED_PWRS_DISCHARGE_FULL) {
			LOG_INF("Skipping LED_PWRS_DISCHARGE_FULL");
			continue;
			// batt_min = 1000;
			// batt_max = 1000;
		} else {
			batt_min = 0;
			batt_max = 1000;
		}

		for (batt_level = batt_min; batt_level <= batt_max;
		     batt_level += 10) {
			found_node = false;
			charge_get_display_charge_fake.return_val = batt_level;
			for (int i = 0; i < LED_POLICY_COUNT; i++) {
				if (match_node(i) != -1) {
					found_node = true;
				}
			}

			if (!found_node) {
				LOG_ERR("LED policy failed: %s, batt_level %d",
					led_pwrs_name[dut_state->pwr_state],
					batt_level);
			}
			zassert_true(found_node);
		}
	}
}

ZTEST(led_policy, test_led_policy_gaps_chipset_suspend)
{
	bool found_node = false;
	int batt_level, batt_min, batt_max;

	chipset_current_mask = CHIPSET_STATE_ANY_SUSPEND;

	for (int state = 0; state < ARRAY_SIZE(led_policy_dut_states);
	     state++) {
		struct dut_state *dut_state = &led_policy_dut_states[state];

		LOG_INF("Policy: %s, chg_port %d, batt_status 0x%08x",
			led_pwrs_name[dut_state->pwr_state],
			dut_state->active_charge_port,
			dut_state->battery_status);

		led_pwr_get_state_fake.return_val = dut_state->pwr_state;
		charge_manager_get_active_charge_port_fake.return_val =
			dut_state->active_charge_port;
		current_battery_status = dut_state->battery_status;

		if (dut_state->pwr_state == LED_PWRS_DISCHARGE_FULL) {
			LOG_INF("Skipping LED_PWRS_DISCHARGE_FULL");
			continue;
			// batt_min = 1000;
			// batt_max = 1000;
		} else {
			batt_min = 0;
			batt_max = 1000;
		}

		for (batt_level = batt_min; batt_level <= batt_max;
		     batt_level += 10) {
			found_node = false;
			for (int i = 0; i < LED_POLICY_COUNT; i++) {
				charge_get_display_charge_fake.return_val =
					batt_level;
				if (match_node(i) != -1) {
					found_node = true;
				}
			}

			if (!found_node) {
				LOG_ERR("LED policy failed: %s, batt_level %d",
					led_pwrs_name[dut_state->pwr_state],
					batt_level);
			}
			zassert_true(found_node);
		}
	}
}
ZTEST(led_policy, test_led_policy_gaps_chipset_off)
{
	bool found_node = false;
	int batt_level, batt_min, batt_max;

	chipset_current_mask = CHIPSET_STATE_ANY_OFF;

	for (int state = 0; state < ARRAY_SIZE(led_policy_dut_states);
	     state++) {
		struct dut_state *dut_state = &led_policy_dut_states[state];

		LOG_INF("Policy: %s, chg_port %d, batt_status 0x%08x",
			led_pwrs_name[dut_state->pwr_state],
			dut_state->active_charge_port,
			dut_state->battery_status);

		led_pwr_get_state_fake.return_val = dut_state->pwr_state;
		charge_manager_get_active_charge_port_fake.return_val =
			dut_state->active_charge_port;
		current_battery_status = dut_state->battery_status;

		if (dut_state->pwr_state == LED_PWRS_DISCHARGE_FULL) {
			LOG_INF("Skipping LED_PWRS_DISCHARGE_FULL");
			continue;
			// batt_min = 1000;
			// batt_max = 1000;
		} else {
			batt_min = 0;
			batt_max = 1000;
		}

		for (batt_level = batt_min; batt_level <= batt_max;
		     batt_level += 10) {
			found_node = false;
			for (int i = 0; i < LED_POLICY_COUNT; i++) {
				charge_get_display_charge_fake.return_val =
					batt_level;
				if (match_node(i) != -1) {
					found_node = true;
				}
			}

			if (!found_node) {
				LOG_ERR("LED policy failed: %s, batt_level %d",
					led_pwrs_name[dut_state->pwr_state],
					batt_level);
			}
			zassert_true(found_node);
		}
	}
}
