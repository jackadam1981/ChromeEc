/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

#include "hooks.h"
#include "motionsense_sensors.h"

#define ALT_MAT		SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_chassis_rot_ref))
#define LID_SENSOR	SENSOR_ID(DT_NODELABEL(lid_accel))

static void update_rotation_matrix(void)
{
	motion_sensors[LID_SENSOR].rot_standard_ref = &ALT_MAT;
}
DECLARE_HOOK(HOOK_INIT, update_rotation_matrix, HOOK_PRIO_POST_I2C);