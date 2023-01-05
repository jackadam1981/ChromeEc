/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_bmi3xx.h"
#include "hooks.h"
#include "motionsense_sensors.h"

void base_accel_interrupt(enum gpio_signal signal)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_BASE_SENSOR, &val);

	if (val == FW_BASE_BMI323)
		bmi3xx_interrupt(signal);
	else if (val == FW_BASE_LIS2DW12)
		lis2dw12_interrupt(signal);
}

static void motionsense_init(void)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_BASE_SENSOR, &val);
	if (val == FW_BASE_BMI323) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_base_accel);
		ccprints("BASE ACCEL is BMI323");
	} else if (val == FW_BASE_LIS2DW12) {
		ccprints("BASE ACCEL IS LIS2DW12");
	}
}
DECLARE_HOOK(HOOK_INIT, motionsense_init, HOOK_PRIO_DEFAULT);
