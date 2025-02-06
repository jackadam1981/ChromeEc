/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic_log.h"

#include <zephyr/ztest.h>

/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic_log.h"

#include <zephyr/ztest.h>

bool panic_log_is_frozen(void);

ZTEST_USER(panic_log, test_reset)
{
	zassert_true(panic_log_is_frozen() == true);
	panic_log_init();
	zassert_true(panic_log_is_frozen() == false);
}

ZTEST_SUITE(panic_log, NULL, NULL, NULL, NULL, NULL);
