/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "multistep_test.h"
#include "panic.h"
#include "system.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(exit_hw_test, LOG_LEVEL_INF);

static void test_exit(void)
{
	LOG_INF("Calling exit\n");
	cflush();
	exit(1);
	/* Should never reach this. */
	zassert_unreachable();
}

static void test_panic_data(void)
{
#ifdef CONFIG_ARM
	struct panic_data *const pdata = panic_get_data();
	uint32_t exit_addr = (uint32_t)exit;
	/* Estimated end of the exit function, which is short. */
	uint32_t exit_end = (uint32_t)exit + 0x40;
	uint32_t pc = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_PC];

	/* Make sure Program Counter is stored correctly and points at the exit
	 * function.
	 */
	zassert_true((exit_addr <= pc) && (exit_end >= pc));
#endif
}

static void (*test_steps[])(void) = { test_exit, test_panic_data };

MULTISTEP_TEST(exit, test_steps)
