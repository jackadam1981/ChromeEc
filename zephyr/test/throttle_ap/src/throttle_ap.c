/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <chipset.h>
#include <gpio.h>
#include <throttle_ap.h>

#include "common.h"
#include <console.h>

#include <zephyr/ztest.h>

static bool is_throttled;
void chipset_throttle_cpu(int throttle)
{
	is_throttled = throttle;
}

ZTEST_USER(throttle_ap, test_throttle_ap)
{
	throttle_ap(THROTTLE_ON, THROTTLE_HARD, THROTTLE_SRC_AC);
	zassert_true(is_throttled);

	throttle_ap(THROTTLE_OFF, THROTTLE_HARD, THROTTLE_SRC_AC);
	zassert_false(is_throttled);
}
