/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ztest.h>

#include "chipset.h"
#include "common.h"
#include "host_command.h"
#include "power.h"
#include "stubs.h"
#include "task.h"

/** Test chipset_in_state() for each state */
static void test_power_chipset_in_state(void)
{
	int all_state_masks = CHIPSET_STATE_HARD_OFF | CHIPSET_STATE_SOFT_OFF |
			      CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON |
			      CHIPSET_STATE_STANDBY;
	struct {
		/* Selected power state */
		enum power_state p_state;
		/* Selected state as CHIPSET_STATE_* */
		int state;
	} test_param[] = {
		{
			.p_state = POWER_G3,
			.state = CHIPSET_STATE_HARD_OFF,
		},
		{
			.p_state = POWER_G3S5,
			.state = CHIPSET_STATE_HARD_OFF |
				 CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S5G3,
			.state = CHIPSET_STATE_HARD_OFF |
				 CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S5,
			.state = CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S5S3,
			.state = CHIPSET_STATE_SOFT_OFF |
				 CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S3S5,
			.state = CHIPSET_STATE_SOFT_OFF |
				 CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S3,
			.state = CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S3S0,
			.state = CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON,
		},
		{
			.p_state = POWER_S0S3,
			.state = CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON,
		},
		{
			.p_state = POWER_S0,
			.state = CHIPSET_STATE_ON,
		},
	};
	bool should_be_in_state;
	bool is_in_state;
	int mask;

	for (int i = 0; i < ARRAY_SIZE(test_param); i++) {
		/* Set given power state */
		power_set_state(test_param[i].p_state);
		/* Test with all possible state masks */
		for (mask = 0; mask <= all_state_masks; mask++) {
			/* Currently tested mask match with state */
			should_be_in_state = (mask & test_param[i].state) ==
					     test_param[i].state;
			is_in_state = chipset_in_state(mask);
			zassert_equal(should_be_in_state, is_in_state,
				      "Wrong chipset_in_state() == %d, "
				      "should be %d; mask 0x%x; power state %d "
				      "in test case %d",
				      is_in_state, should_be_in_state, mask,
				      test_param[i].p_state, i);
		}
	}
}

/** Test chipset_in_state() for each state */
static void test_power_chipset_in_or_transitioning_to_state(void)
{
	int all_state_masks = CHIPSET_STATE_HARD_OFF | CHIPSET_STATE_SOFT_OFF |
			      CHIPSET_STATE_SUSPEND | CHIPSET_STATE_ON |
			      CHIPSET_STATE_STANDBY;
	struct {
		/* Selected power state */
		enum power_state p_state;
		/* Selected state as CHIPSET_STATE_* */
		int state;
	} test_param[] = {
		{
			.p_state = POWER_G3,
			.state = CHIPSET_STATE_HARD_OFF,
		},
		{
			.p_state = POWER_G3S5,
			.state = CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S5G3,
			.state = CHIPSET_STATE_HARD_OFF,
		},
		{
			.p_state = POWER_S5,
			.state = CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S5S3,
			.state = CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S3S5,
			.state = CHIPSET_STATE_SOFT_OFF,
		},
		{
			.p_state = POWER_S3,
			.state = CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S3S0,
			.state = CHIPSET_STATE_ON,
		},
		{
			.p_state = POWER_S0S3,
			.state = CHIPSET_STATE_SUSPEND,
		},
		{
			.p_state = POWER_S0,
			.state = CHIPSET_STATE_ON,
		},
	};
	bool should_be_in_state;
	bool is_in_state;
	int mask;

	for (int i = 0; i < ARRAY_SIZE(test_param); i++) {
		/* Set given power state */
		power_set_state(test_param[i].p_state);
		/* Test with all possible state masks */
		for (mask = 0; mask <= all_state_masks; mask++) {
			/* Currently tested mask match with state */
			should_be_in_state = mask & test_param[i].state;
			is_in_state = chipset_in_or_transitioning_to_state(
									mask);
			zassert_equal(should_be_in_state, is_in_state,
				      "Wrong "
				      "chipset_in_or_transitioning_to_state() "
				      "== %d, should be %d; mask 0x%x; "
				      "power state %d in test case %d",
				      is_in_state, should_be_in_state, mask,
				      test_param[i].p_state, i);
		}
	}
}

/** Test using chipset_exit_hard_off() in different power states */
static void test_power_exit_hard_off(void)
{

	/* Force initial state */
	force_power_state(true, POWER_G3);
	zassert_equal(POWER_G3, power_get_state(), NULL);

	/* Stop forcing state */
	force_power_state(false, 0);

	/* Test after exit hard off, we reach G3S5 */
	chipset_exit_hard_off();
	/*
	 * TODO(b/201420132) - chipset_exit_hard_off() is waking up
	 * TASK_ID_CHIPSET Sleep is required to run chipset task before
	 * continuing with test
	 */
	k_msleep(1);
	zassert_equal(POWER_G3S5, power_get_state(), NULL);

	/* Go back to G3 and check we stay there */
	force_power_state(true, POWER_G3);
	force_power_state(false, 0);
	zassert_equal(POWER_G3, power_get_state(), NULL);

	/* Exit G3 again */
	chipset_exit_hard_off();
	/* TODO(b/201420132) - see comment above */
	k_msleep(1);
	zassert_equal(POWER_G3S5, power_get_state(), NULL);

	/* Go to S5G3 */
	force_power_state(true, POWER_S5G3);
	zassert_equal(POWER_S5G3, power_get_state(), NULL);

	/* Test exit hard off in S5G3 -- should immedietly exit G3 */
	chipset_exit_hard_off();
	/* Go back to G3 and check we exit it to G3S5 */
	force_power_state(true, POWER_G3);
	zassert_equal(POWER_G3S5, power_get_state(), NULL);

	/* Test exit hard off is cleared on entering S5 */
	chipset_exit_hard_off();
	force_power_state(true, POWER_S5);
	zassert_equal(POWER_S5, power_get_state(), NULL);
	/* Go back to G3 and check we stay in G3 */
	force_power_state(true, POWER_G3);
	force_power_state(false, 0);
	zassert_equal(POWER_G3, power_get_state(), NULL);

	/* Test exit hard off doesn't work on other states */
	force_power_state(true, POWER_S5S3);
	force_power_state(false, 0);
	zassert_equal(POWER_S5S3, power_get_state(), NULL);
	chipset_exit_hard_off();
	/* TODO(b/201420132) - see comment above */
	k_msleep(1);

	/* Go back to G3 and check we stay in G3 */
	force_power_state(true, POWER_G3);
	force_power_state(false, 0);
	zassert_equal(POWER_G3, power_get_state(), NULL);
}

/* Test reboot ap on g3 host command is triggering reboot */
static void test_power_reboot_ap_at_g3(void)
{
	struct ec_params_reboot_ap_on_g3_v1 params;
	struct host_cmd_handler_args args = {
		.command = EC_CMD_REBOOT_AP_ON_G3,
		.version = 0,
		.send_response = stub_send_response_callback,
		.params = &params,
		.params_size = sizeof(params),
	};
	int delay;

	/* Force initial state S0 */
	force_power_state(true, POWER_S0);
	zassert_equal(POWER_S0, power_get_state(), NULL);

	/* Test version 0 (no delay argument) */
	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);

	/* Go to G3 and check if reboot is triggered */
	force_power_state(true, POWER_G3);
	zassert_equal(POWER_G3S5, power_get_state(), NULL);

	/* Test version 1 (with delay argument) */
	args.version = 1;
	delay = 3000;
	params.reboot_ap_at_g3_delay = delay / 1000; /* in seconds */
	zassert_equal(EC_RES_SUCCESS, host_command_process(&args), NULL);

	/* Go to G3 and check if reboot is triggered after delay */
	force_power_state(true, POWER_G3);
	force_power_state(false, 0);
	zassert_equal(POWER_G3, power_get_state(), NULL);
	k_msleep(delay - 50);
	/* Test if still in G3 */
	zassert_equal(POWER_G3, power_get_state(), NULL);
	/*
	 * power_common_state() use for loop with 100ms sleeps. msleep() wait at
	 * least specified time, so wait 10% longer than specified delay to take
	 * this into account.
	 */
	k_msleep(50 + delay / 10);
	/* Test if reboot is triggered */
	zassert_equal(POWER_G3S5, power_get_state(), NULL);
}

void test_suite_power_common(void)
{
	ztest_test_suite(power_common,
			 ztest_unit_test(test_power_chipset_in_state),
			 ztest_unit_test(
			       test_power_chipset_in_or_transitioning_to_state),
			 ztest_unit_test(test_power_exit_hard_off),
			 ztest_unit_test(test_power_reboot_ap_at_g3));
	ztest_run_test_suite(power_common);
}
