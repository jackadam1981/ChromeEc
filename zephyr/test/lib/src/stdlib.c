/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include <strings.h>

/* Test only stdlib functions implemented in EC (ec/builtin/stdlib.c), that are
 * used with Zephyr.
 */
ZTEST_SUITE(stdlib, NULL, NULL, NULL, NULL, NULL);

ZTEST(stdlib, test_strcasecmp)
{
	zassert_true(strcasecmp("test string", "TEST strIng") == 0);
	zassert_true(strcasecmp("test123!@#", "TesT123!@#") == 0);
	zassert_true(strcasecmp("lower", "UPPER") != 0);
}
