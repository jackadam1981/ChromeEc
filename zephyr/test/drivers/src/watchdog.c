/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/watchdog.h>
#include <logging/log.h>
#include <zephyr.h>
#include <ztest.h>

#include "config.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "watchdog.h"

/** Setup watchdog  */
static void setup_watchdog(void)
{
	//set_test_runner_tid();
}

/** Restore watchdog */
static void restore_watchdog(void)
{
	/* TODO */
}

/** Test watchdog init */
static void test_watchdog_init(void)
{
	int retval = EC_SUCCESS;

	/* Test successful initialization */
	//retval = watchdog_init();
	zassert_equal(EC_SUCCESS, retval,
				"Expected EC_SUCCESS, returned %d", retval);
}

void test_suite_watchdog(void)
{
	ztest_test_suite(watchdog,
			 ztest_unit_test_setup_teardown(test_watchdog_init,
			 setup_watchdog,
			 restore_watchdog));

	ztest_run_test_suite(watchdog);
}
