/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_cbi.h"
#include "hooks.h"
#include "motionsense_sensors.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kaladin_sensor, LOG_LEVEL_INF);

#define ALT_MAT SENSOR_ROT_STD_REF_NAME(DT_NODELABEL(lid_rot_ref_2))
#define LID_SENSOR SENSOR_ID(DT_NODELABEL(lid_accel))

static void sensor_init(void)
{
	int ret;
	uint32_t val;
	/*
	 * If the firmware config indicates
	 * For DBTS panel, use the alternative
	 * rotation matrix.
	 */
	ret = cros_cbi_get_fw_config(FW_PANEL, &val);
	if (ret != 0) {
		LOG_INF("Error retrieving CBI FW_CONFIG field %d", FW_PANEL);
		return;
	}

	if (val == PANEL_DBTS) {
		LOG_INF("Switching to DBTS rotation matrix");
		motion_sensors[LID_SENSOR].rot_standard_ref = &ALT_MAT;
	}
}
DECLARE_HOOK(HOOK_INIT, sensor_init, HOOK_PRIO_POST_I2C);
