/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "keyboard_scan.h"
#include "motion_sense.h"
#include "hooks.h"
#include "tablet_mode.h"
#include "sku.h"

/* Lazor board specific sensor implementation */

#ifdef CONFIG_LID_ANGLE_UPDATE
void lid_angle_peripheral_enable(int enable)
{
	int chipset_in_s0 = chipset_in_state(CHIPSET_STATE_ON);

	if (enable) {
		keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_ANGLE);
	} else {
		/*
		 * Ensure that the chipset is off before disabling the keyboard.
		 * When the chipset is on, the EC keeps the keyboard enabled and
		 * the AP decides whether to ignore input devices or not.
		 */
		if (!chipset_in_s0)
			keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_ANGLE);
	}
}
#endif

static void board_update_sensor_config_from_sku(void)
{
	if (board_is_clamshell()) {
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
		/* The sensors are not stuffed; don't allow lines to float */
		gpio_set_flags(GPIO_ACCEL_GYRO_INT_L,
			       GPIO_INPUT | GPIO_PULL_DOWN);
		gpio_set_flags(GPIO_LID_ACCEL_INT_L,
			       GPIO_INPUT | GPIO_PULL_DOWN);
	} else {
		/* TODO: This part has to be done with non-Lizmuzeen board */
//		board_detect_motionsensor();
//		/* Enable interrupt for the base accel sensor */
//		gpio_enable_interrupt(GPIO_ACCEL_GYRO_INT_L);
	}
}
DECLARE_HOOK(HOOK_INIT, board_update_sensor_config_from_sku,
	     HOOK_PRIO_INIT_I2C + 2);
