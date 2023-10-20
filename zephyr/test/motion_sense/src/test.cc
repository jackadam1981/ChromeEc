/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "motion_sense.h"

#include <zephyr/ztest.h>

ZTEST_SUITE(motion_sense, NULL, NULL, NULL, NULL, NULL);

ZTEST(motion_sense, test_stub)
{
	zassert_equal(motion_sensor_count, 2);
}
