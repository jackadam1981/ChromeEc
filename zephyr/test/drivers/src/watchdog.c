/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/watchdog.h>

#include <logging/log.h>
#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "ec_tasks.h"
#include "hooks.h"
#include "stubs.h"
#include "watchdog.h"

#define wdt DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog))

static bool wdt_timer_expired;

static void duration_expire(struct k_timer *timer)
{
	wdt_timer_expired = true;
}

/** TESTPOINT: init timer via K_TIMER_DEFINE */
K_TIMER_DEFINE(ktimer, duration_expire, NULL);

#define DEFAULT_WDT_TIMEOUT_MS \
	(CONFIG_AUX_TIMER_PERIOD_MS + (CONFIG_AUX_TIMER_PERIOD_MS / 2))

/** Setup watchdog  */
static void setup_watchdog(void)
{
	set_test_runner_tid();
	wdt_timer_expired = false;
}

/** Restore watchdog */
static void teardown_watchdog(void)
{
	/* TODO */
}

/** Test watchdog init */
static void test_watchdog_init(void)
{
	int retval = EC_SUCCESS;
	const struct device *wdt_dev = wdt;

	zassert_not_null(wdt_dev, "watchdog is NULL");

	/* Test successful initialization */
	retval = watchdog_init();
	zassert_equal(-ENOMEM, retval,
				"Expected ENOMEM, returned %d", retval);
}

/** Test watchdog reload */
static void test_watchdog_reload(void)
{
	watchdog_reload();
}

/** Test watchdog warning handler */
static void test_wdt_warning_handler(void)
{
	k_timer_start(&ktimer, K_MSEC(DEFAULT_WDT_TIMEOUT_MS), K_NO_WAIT);
	k_busy_wait(DEFAULT_WDT_TIMEOUT_MS * 1000);
	k_timer_stop(&ktimer);

	zassert_true(wdt_timer_expired, "ktimer didn't expire");
}

void test_suite_watchdog(void)
{
	ztest_test_suite(watchdog,
			 ztest_unit_test_setup_teardown(
				test_watchdog_init,
				setup_watchdog,
				teardown_watchdog),
			 ztest_unit_test_setup_teardown(
				test_watchdog_reload,
				setup_watchdog,
				teardown_watchdog),
			 ztest_unit_test_setup_teardown(
				test_wdt_warning_handler,
				setup_watchdog,
				teardown_watchdog));

	ztest_run_test_suite(watchdog);
}
