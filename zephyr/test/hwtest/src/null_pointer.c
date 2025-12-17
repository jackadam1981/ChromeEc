/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

LOG_MODULE_REGISTER(null_pointer, LOG_LEVEL_INF);

ZTEST_SUITE(null_pointer, NULL, NULL, NULL, NULL, NULL);

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	zassert_equal(reason, K_ERR_CPU_EXCEPTION);
#if defined(CONFIG_EXTRA_EXCEPTION_INFO) && defined(CONFIG_ARM)
	zassert_true((pEsf->extra_info.cfsr & 0xff) == 0x82,
		     "Expected a data access violation, but cfsr is 0x%x",
		     pEsf->extra_info.cfsr);
	zassert_equal(pEsf->extra_info.mmfar, 0,
		      "Expected mmfar to be 0, but was 0x%x",
		      pEsf->extra_info.mmfar);
#endif
	ztest_set_fault_valid(false);
}

void null_pointer_dereference(void)
{
	volatile uint32_t *null_ptr = NULL;

	ztest_set_fault_valid(true);
	LOG_INF("The value of null_ptr after dereferencing is: %d", *null_ptr);

	/* Should never reach this. */
	zassert_unreachable();
}

ZTEST(null_pointer, test_null_pointer_dereference)
{
	null_pointer_dereference();

	/* Should never reach this. */
	zassert_unreachable();
}
