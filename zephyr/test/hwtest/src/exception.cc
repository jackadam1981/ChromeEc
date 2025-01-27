/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "system.h"

#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

#include <exception>

LOG_MODULE_REGISTER(exception_hw_test, LOG_LEVEL_INF);

ZTEST_SUITE(exception_hw_test, NULL, NULL, NULL, NULL, NULL);

void exception_lib_throw(void);

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	LOG_INF("Caught system error -- reason %d\n", reason);
	cflush();
	zassert_equal(reason, K_ERR_KERNEL_PANIC);
	ztest_test_pass();
}

ZTEST(exception_hw_test, test_exception)
{
	LOG_INF("Throwing an exception\n");
	cflush();
	ztest_set_fault_valid(true);
	exception_lib_throw();

	/*
	 * Since we have exceptions disabled, we should not reach this.
	 * Instead, the exception should cause a reboot.
	 */
	ztest_test_fail();
}
