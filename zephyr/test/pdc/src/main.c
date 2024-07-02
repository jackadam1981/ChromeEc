/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "hooks.h"
#include "test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

void test_main(void)
{
	struct test_state state = {
		.ec_app_main_run = false,
	};

	ztest_set_fault_valid(true);

	/* Run all the suites that depend on main not being called yet */
	ztest_run_test_suites(&state, false, 1, 1);

	ec_app_main();

	k_sleep(K_MSEC(1000));

	/* Run all the suites that depend on main being called */
	ztest_run_test_suites(NULL, false, 1, 1);

	/* Check that every suite ran */
	ztest_verify_all_test_suites_ran();
}
