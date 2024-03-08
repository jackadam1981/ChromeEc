/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_app_main.h"
#include "hooks.h"
#include "test_state.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

/**
 * @brief Semaphore that signals when hooks have completed
 */
static struct k_sem init_hooks_completed;

/**
 * @brief Hook callback function. Gets registered with the lowest priority so
 *        that we know all actual hooks have finished. Increments the semaphore.
 */
static void hook_completed_callback(void)
{
	/* Signal that hooks are completed */
	k_sem_give(&init_hooks_completed);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, hook_completed_callback, HOOK_PRIO_LAST);

static bool rex_predicate_pre_main(const void *state)
{
	return ((struct test_state *)state)->ec_app_main_run == false;
}

bool rex_predicate_post_main(const void *state)
{
	return !rex_predicate_pre_main(state);
}

void test_main(void)
{
	k_sem_init(&init_hooks_completed, 0, 1);

	struct test_state state = {
		.ec_app_main_run = false,
	};

	/* Run all the suites that depend on main not being called yet */
	ztest_run_all(&state, false, 1, 1);

	ec_app_main();
	state.ec_app_main_run = true;

	/* Run all the suites that depend on main being called */
	ztest_run_all(&state, false, 1, 1);
}
