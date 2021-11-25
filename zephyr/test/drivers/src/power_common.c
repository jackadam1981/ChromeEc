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

void test_suite_power_common(void)
{
	ztest_test_suite(power_common,
			 ztest_unit_test(test_power_chipset_in_state),
			 ztest_unit_test(
			       test_power_chipset_in_or_transitioning_to_state)
			);
	ztest_run_test_suite(power_common);
}
