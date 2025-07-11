/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "system.h"
#include "util.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/ztest.h>

LOG_MODULE_REGISTER(null_pointer, LOG_LEVEL_INF);

ZTEST_SUITE(null_pointer, NULL, NULL, NULL, NULL, NULL);

ZTEST(null_pointer, test_dereference)
{
	volatile uint32_t *null_ptr = NULL;
	LOG_INF("The value of null_ptr after dereferencing is: %d", *null_ptr);

	if (!IS_ENABLED(CONFIG_NULL_POINTER_EXCEPTION_DETECTION_NONE)) {
		/* Should never reach this. */
		zassert_unreachable();
	}
}
