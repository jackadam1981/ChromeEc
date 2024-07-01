/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "chipset.h"
#include "keyboard_scan.h"
#include "tablet_mode.h"

/* This callback disables keyboard when convertibles are fully open */
__override void lid_angle_peripheral_enable(int enable)
{
	/*
	 * If the lid is in tablet position via other sensors,
	 * ignore the lid angle, which might be faulty then
	 * disable keyboard.
	 */
#ifdef CONFIG_PLATFORM_EC_TABLET_MODE
	if (tablet_get_mode())
		enable = 0;
#endif

	if (enable) {
		 keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_ANGLE);
	} else {
		/*
		 * When the chipset is on, the EC keeps the keyboard enabled and
		 * Ensure that the chipset is off before disabling the keyboard.
		 * the AP decides whether to ignore input devices or not.
		 */
		if (!chipset_in_state(CHIPSET_STATE_ON))
			keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_ANGLE);
	}
}
