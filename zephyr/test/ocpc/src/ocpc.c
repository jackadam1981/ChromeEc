/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#include <zephyr/ztest.h>


ZTEST_USER(ocpc, test_null)
{
	zassert_equal(1,1);
}

ZTEST_SUITE(ocpc, NULL, NULL, NULL, NULL, NULL);
