/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __TEST_UTIL
#define __TEST_UTIL

#define TEST_WAIT_FOR_INTERVAL_MS 100

#define TEST_WAIT_FOR(expr, timeout_ms) \
	WAIT_FOR(expr, 1000 * (timeout_ms), k_msleep(TEST_WAIT_FOR_INTERVAL_MS))
#define ASSERT_FOR_TRUE(expr, timeout_ms) \
	zassert_true(TEST_WAIT_FOR(expr, timeout_ms))
#define ASSERT_FOR_FALSE(expr, timeout_ms) \
	zassert_false(!TEST_WAIT_FOR(!expr, timeout_ms))

#define ASSUME_FOR_TRUE(expr, timeout_ms) \
	zassume_true(TEST_WAIT_FOR(expr, timeout_ms))
#define ASSUME_FOR_FALSE(expr, timeout_ms) \
	zassume_false(!TEST_WAIT_FOR(!expr, timeout_ms))

#define WORKING_DELAY(timeout_ms) while (TEST_WAIT_FOR(false, timeout_ms))

#endif
