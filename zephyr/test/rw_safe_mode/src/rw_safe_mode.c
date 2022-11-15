/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "rw_safe_mode.h"
#include "system.h"
#include "system_fake.h"
#include "ec_tasks.h"
#include "rw_safe_mode.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/fff.h>

FAKE_VOID_FUNC(system_reset, int);

static void system_before(void *data)
{
	RESET_FAKE(system_reset);
	set_safe_mode(false);
	system_set_shrspi_image_copy(EC_IMAGE_RW);
}

void enter_safe_mode_cb(struct k_timer *unused)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	zassert_true(start_rw_safe_mode() != EC_SUCCESS);
	zassert_equal(system_is_in_rw_safe_mode(), false);
}
K_TIMER_DEFINE(enter_safe_mode, enter_safe_mode_cb, NULL);

ZTEST_USER(rw_safe_mode, test_safe_mode_from_critical_task)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	/**
	 * Timer callback will run in sysworkq, which is a critical
	 * thread, so safe mode should not run.
	 */
	k_timer_start(&enter_safe_mode, K_NO_WAIT, K_NO_WAIT);
	/* Short wait to ensure enter_safe_mode_cb has a chance to run */
	k_msleep(1);
}

ZTEST_USER(rw_safe_mode, test_enter_safe_mode_from_ro)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	system_set_shrspi_image_copy(EC_IMAGE_RO);
	zassert_true(start_rw_safe_mode() != EC_SUCCESS);
	zassert_equal(system_is_in_rw_safe_mode(), false);
}

ZTEST_USER(rw_safe_mode, test_enter_safe_mode_twice)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	zassert_ok(start_rw_safe_mode());
	zassert_equal(system_is_in_rw_safe_mode(), true);
	zassert_true(start_rw_safe_mode() != EC_SUCCESS);
	zassert_equal(system_is_in_rw_safe_mode(), true);
}

ZTEST_USER(rw_safe_mode, test_enter_safe_mode)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	zassert_ok(start_rw_safe_mode());
	zassert_equal(system_is_in_rw_safe_mode(), true);
}

ZTEST_USER(rw_safe_mode, test_safe_mode_reboot)
{
	zassert_equal(system_is_in_rw_safe_mode(), false);
	zassert_ok(start_rw_safe_mode());
	zassert_equal(system_is_in_rw_safe_mode(), true);
	zassert_equal(0, system_reset_fake.call_count,
		      "Expected system_reset() to be called 0 times, but was "
		      "called %d times",
		      system_reset_fake.call_count);
	k_msleep(SAFE_MODE_TIMEOUT_MSEC);
	zassert_equal(1, system_reset_fake.call_count,
		      "Expected system_reset() to be called once, but was "
		      "called %d times",
		      system_reset_fake.call_count);
}

ZTEST_USER(rw_safe_mode, test_blocked_command_in_safe_mode)
{
	struct ec_params_gpio_get cmd_params = {
		.name = "wp_l",
	};
	struct ec_response_gpio_get cmd_response;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND(
		EC_CMD_GPIO_GET, 0, cmd_response, cmd_params);

	zassert_equal(system_is_in_rw_safe_mode(), false);
	zassert_ok(host_command_process(&args));

	zassert_ok(start_rw_safe_mode());

	zassert_equal(system_is_in_rw_safe_mode(), true);
	zassert_true(host_command_process(&args));
}

ZTEST_SUITE(rw_safe_mode, NULL, NULL, system_before, NULL, NULL);
