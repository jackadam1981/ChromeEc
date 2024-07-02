/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "test_state.h"

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_error_hook.h>

void ztest_post_fatal_error_hook(unsigned int reason,
				 const struct arch_esf *pEsf)
{
	/* check if expected error */
	zassert_equal(reason, K_ERR_KERNEL_OOPS);
}

bool predicate_pre_main(const void *state)
{
	return ((struct test_state *)state)->ec_app_main_run == false;
}

bool predicate_post_main(const void *state)
{
	return !predicate_pre_main(state);
}

ZTEST_SUITE(pdc_device_not_ready, predicate_pre_main, NULL, NULL, NULL, NULL);

ZTEST_USER(pdc_device_not_ready, test_pdc_device_not_ready)
{
	ztest_set_fault_valid(true);
}
