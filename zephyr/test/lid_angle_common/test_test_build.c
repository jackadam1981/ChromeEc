/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "keyboard_scan.h"
#include "lid_angle.h"
#include "tablet_mode.h"

#include <zephyr/fff.h>
#include <zephyr/ztest.h>

ZTEST(lid_angle_common, test_enable)
{
	lid_angle_peripheral_enable(1);
	zassert_equal(0, keyboard_scan_enable_fake.call_count);
}

ZTEST(lid_angle_common, test_disable)
{
	lid_angle_peripheral_enable(0);
	zassert_equal(0, keyboard_scan_enable_fake.call_count);
}
