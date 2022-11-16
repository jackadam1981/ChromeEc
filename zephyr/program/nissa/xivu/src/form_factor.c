/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/devicetree.h>

#include "accelgyro.h"
#include "button.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/accel_bma4xx.h"
#include "driver/accel_lis2dw12_public.h"
#include "driver/accelgyro_bmi323.h"
#include "driver/accelgyro_lsm6dso.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motionsense_sensors.h"
#include "motion_sense.h"
#include "tablet_mode.h"

//LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

/*
 * Mainboard orientation support.
 */

#define LID_SENSOR SENSOR_ID(DT_NODELABEL(lid_accel))
#define BASE_SENSOR SENSOR_ID(DT_NODELABEL(base_accel))
#define BASE_GYRO SENSOR_ID(DT_NODELABEL(base_gyro))
#define ALT_LID_S SENSOR_ID(DT_NODELABEL(alt_lid_accel))

void motion_interrupt(enum gpio_signal signal)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_BASE_GYRO, &val);

	if (val == FW_BASE_BMI323)
		bmi3xx_interrupt(signal);
	else
		lsm6dso_interrupt(signal);
}

void lid_accel_interrupt(enum gpio_signal signal)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_LID_G, &val);

	if (val == FW_LID_BMA422)
		bma4xx_interrupt(signal);
	else
		lis2dw12_interrupt(signal);
}

static void motionsense_init(void)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_BASE_GYRO, &val);

	if (val == FW_BASE_BMI323) {
		ccprints("BASE ACCEL is BMI323");
	} else if (val == FW_BASE_ISM6DSO) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_base_accel);
		MOTIONSENSE_ENABLE_ALTERNATE(alt_base_gyro);
		ccprints("BASE ACCEL IS ISM6DSO");
	} else {
		ccprints("no gyro");
	}

	cros_cbi_get_fw_config(FW_LID_G, &val);

	if (val == FW_LID_BMA422) {
		ccprints("BASE ACCEL is BMA422");
	} else if (val == FW_LID_LIS2DW12) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_lid_accel);
		ccprints("LID G IS LIS2DW12");
	} else {
		ccprints("no gyro");
	}
}
DECLARE_HOOK(HOOK_INIT, motionsense_init, HOOK_PRIO_DEFAULT);
