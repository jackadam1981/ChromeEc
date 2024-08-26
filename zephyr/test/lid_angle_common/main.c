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

DEFINE_FFF_GLOBALS;

static void lid_angle_common_before(void *f)
{
	RESET_FAKE(chipset_in_state);
	RESET_FAKE(keyboard_scan_enable);
	RESET_FAKE(tablet_get_mode);
	FFF_RESET_HISTORY();
}

ZTEST_SUITE(lid_angle_common, NULL, NULL, lid_angle_common_before, NULL, NULL);
