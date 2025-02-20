/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "system.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(libc_exit, LOG_LEVEL_INF);


ZTEST_SUITE(libc_exit, NULL, NULL, NULL, NULL, NULL);

extern void exit(int rc);


void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	zassert_equal(reason, K_ERR_KERNEL_PANIC);
	ztest_test_pass();
}


ZTEST_USER(libc_exit, test_exit)
{
	ztest_set_fault_valid(true);
	exit(1);

	ztest_test_fail();
}
