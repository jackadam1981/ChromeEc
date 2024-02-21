/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "zephyr/kernel.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(always_memset, NULL, NULL, NULL, NULL, NULL);

/* Crytoc is not supported with Zephyr, so add this test for compatibility. */
ZTEST(always_memset, test_always_memset)
{
	zassert_equal(1, 1);
}
