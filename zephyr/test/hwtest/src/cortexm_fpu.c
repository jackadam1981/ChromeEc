/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "zephyr/kernel.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(cortexm_fpu, NULL, NULL, NULL, NULL, NULL);

/* Zephyr doesn't handle FPU IRQ, so add this test for compatibility.*/
ZTEST(cortexm_fpu, test_cortexm_fpu)
{
	zassert_equal(1, 1);
}
