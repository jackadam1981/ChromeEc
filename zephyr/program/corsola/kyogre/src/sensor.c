/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "motion_lid.h"
#include "tablet_mode.h"

#include <dt-bindings/gpio_defines.h>

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

#define MODE_UNKNOWN 0
#define MODE_CLAMSHELL 1
#define MODE_TABLET 2

int tablet_mode = MODE_UNKNOWN;

int board_sensor_at_360(void)
{
	/*
	 * The 360 degree sensor is too sensitive and is active when the lid is
	 * closed at 0 degrees. Ignore the hall sensor when the lid close is
	 * also active.
	 * Additionally, ignore the hall sensor when the lid angle is between
	 * wake_small_angle(13) and wake_large_angle(180).
	 */
	if (tablet_mode == MODE_CLAMSHELL) {
		return 0;
	} else {
		return lid_is_open() && !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(
						gpio_tablet_mode_l));
	}
}

/* This callback disables keyboard when convertibles are fully open */
__override void lid_angle_peripheral_enable(int enable)
{
	int chipset_in_s0 = chipset_in_state(CHIPSET_STATE_ON);

	if (enable) {
		keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_ANGLE);

		if (tablet_mode != MODE_CLAMSHELL) {
			tablet_mode = MODE_CLAMSHELL;
			gmr_tablet_switch_isr(GPIO_TABLET_MODE_L);
		}
	} else {
		/*
		 * Ensure that the chipset is off before disabling the keyboard.
		 * When the chipset is on, the EC keeps the keyboard enabled and
		 * the AP decides whether to ignore input devices or not.
		 */
		if (!chipset_in_s0)
			keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_ANGLE);
		if (tablet_mode != MODE_TABLET) {
			tablet_mode = MODE_TABLET;
			gmr_tablet_switch_isr(GPIO_TABLET_MODE_L);
		}
	}
}
