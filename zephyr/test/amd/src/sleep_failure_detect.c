/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "power.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VOID_FUNC(chipset_reset, enum chipset_shutdown_reason);
FAKE_VOID_FUNC(chipset_force_shutdown, enum chipset_shutdown_reason);

void reset_hang_counters(void);

static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	RESET_FAKE(chipset_reset);
	RESET_FAKE(chipset_force_shutdown);

	reset_hang_counters();
}

ZTEST_SUITE(sleep_failure_detect, NULL, NULL, before, NULL, NULL);

static void run_sleep_failure_detect(enum sleep_hang_type hang_type)
{
	/* After a timeout the EC will attempt to reset the AP. */
	power_board_handle_sleep_hang(hang_type);
	k_msleep(CONFIG_POWER_SLEEP_FAILURE_DETECTION_RESET_MS);
	/* Additional time to make sure the timeout code has run. */
	k_msleep(100);
	zassert_equal(chipset_reset_fake.call_count, 1);
	zassert_equal(chipset_reset_fake.arg0_val, CHIPSET_RESET_HANG_REBOOT);

	/*
	 * If the AP still hasn't reset aften the timeout then the EC will
	 * perform a shutdown.
	 */
	k_msleep(CONFIG_POWER_SLEEP_FAILURE_DETECTION_RESET_MS);
	/* Additional time to make sure the timeout code has run. */
	k_msleep(100);
	zassert_equal(chipset_force_shutdown_fake.call_count, 1);
	zassert_equal(chipset_force_shutdown_fake.arg0_val,
		      CHIPSET_SHUTDOWN_BOARD_CUSTOM);
}

ZTEST(sleep_failure_detect, test_suspend)
{
	run_sleep_failure_detect(SLEEP_HANG_S0IX_SUSPEND);
}

ZTEST(sleep_failure_detect, test_resume)
{
	run_sleep_failure_detect(SLEEP_HANG_S0IX_RESUME);
}
