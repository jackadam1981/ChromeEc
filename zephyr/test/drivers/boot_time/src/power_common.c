/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "host_command.h"
#include "power.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"
#include "timer.h"

#include <string.h>

#include <zephyr/shell/shell_dummy.h>
#include <zephyr/ztest.h>

static void time_mock_after(void *state)
{
	get_time_mock = NULL;
}

/**
 * Test boot time is set
 */
ZTEST(boot_time_no_tasks, test_boot_time_set)
{
	uint64_t time_S5 = 0;
	uint64_t time_S0 = 0;
	timestamp_t fake_time;
	get_time_mock = &fake_time;

	fake_time.val = 0;

	on_new_signal_or_state(POWER_G3, 0);
	power_set_state(POWER_G3);

	zassert_ok(get_boot_time(POWER_S5, &time_S5));
	zassert_ok(get_boot_time(POWER_S0, &time_S0));
	zassert_equal(time_S5, (uint64_t)-1, "time_S5=%llu", time_S5);
	zassert_equal(time_S0, (uint64_t)-1, "time_S0=%llu", time_S0);

	fake_time.val = USEC_PER_SEC;

	on_new_signal_or_state(POWER_S5, 0);
	power_set_state(POWER_S5);

	zassert_ok(get_boot_time(POWER_S5, &time_S5));
	zassert_ok(get_boot_time(POWER_S0, &time_S0));
	zassert_equal(time_S5, USEC_PER_SEC, "time_S5=%llu", time_S5);
	zassert_equal(time_S0, (uint64_t)-1, "time_S0=%llu", time_S0);

	fake_time.val = 2 * USEC_PER_SEC;

	on_new_signal_or_state(POWER_S0, 0);
	power_set_state(POWER_S0);

	zassert_ok(get_boot_time(POWER_S5, &time_S5));
	zassert_ok(get_boot_time(POWER_S0, &time_S0));
	zassert_equal(time_S5, USEC_PER_SEC, "time_S5=%llu", time_S5);
	zassert_equal(time_S0, 2 * USEC_PER_SEC, "time_S0=%llu", time_S0);
}

/**
 * Test boottime ec console command
 */
ZTEST_USER(boot_time, test_boot_time_console_cmd)
{
	int64_t time_S5;
	int64_t time_S0;
	CHECK_CONSOLE_CMD("boottime", NULL, EC_ERROR_PARAM_COUNT);
	CHECK_CONSOLE_CMD("boottime 123", NULL, EC_ERROR_PARAM1);

	SCAN_CONSOLE_CMD("boottime S5", EC_SUCCESS, 1, "%*s last S5: %lldms",
			 &time_S5);
	SCAN_CONSOLE_CMD("boottime S0", EC_SUCCESS, 1, "%*s last S0: %lldms",
			 &time_S0);
	zassert_not_equal(time_S5, -1);
	zassert_not_equal(time_S0, -1);
	zassert_true(time_S5 <= time_S0);
}

ZTEST_SUITE(boot_time_no_tasks, drivers_predicate_pre_main, NULL,
	    time_mock_after, NULL, NULL);
ZTEST_SUITE(boot_time, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
