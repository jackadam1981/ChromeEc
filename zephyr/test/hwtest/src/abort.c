/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
#include "system.h"

#include <stdlib.h>

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(abort_hw_test, LOG_LEVEL_INF);

ZTEST_SUITE(abort_hw_test, NULL, NULL, NULL, NULL, NULL);

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	LOG_INF("Caught system error -- reason %d\n", reason);
	cflush();
	zassert_equal(reason, K_ERR_KERNEL_PANIC);
	ztest_test_pass();
}

ZTEST(abort_hw_test, test_abort)
{
	LOG_INF("Calling abort\n");
	cflush();
	ztest_set_fault_valid(true);
	abort();
	/* Should never reach this. */
	ztest_test_fail();
}
