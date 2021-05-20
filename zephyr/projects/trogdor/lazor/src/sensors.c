/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "motion_sense.h"
#include "hooks.h"
#include "tablet_mode.h"
#include "sku.h"

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
		board_detect_motionsensor();
		/* Enable interrupt for the base accel sensor */
		gpio_enable_interrupt(GPIO_ACCEL_GYRO_INT_L);
	}
}
DECLARE_HOOK(HOOK_INIT, board_update_sensor_config_from_sku,
	     HOOK_PRIO_INIT_I2C + 2);
