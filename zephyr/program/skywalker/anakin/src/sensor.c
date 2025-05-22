/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_cbi.h"
#include "driver/accel_bma4xx.h"
#include "driver/accel_lis2dw12_public.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "hooks.h"
#include "motionsense_sensors.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_sensor, LOG_LEVEL_INF);

static bool lid_uses_bma422;
static bool base_uses_bmi323;

void lid_accel_interrupt(enum gpio_signal signal)
{
	if (lid_uses_bma422) {
		bma4xx_interrupt(signal);
	} else {
		lis2dw12_interrupt(signal);
	}
}

void base_accel_gyro_interrupt(enum gpio_signal signal)
{
	if (base_uses_bmi323) {
		bmi3xx_interrupt(signal);
	} else {
		lsm6dsm_interrupt(signal);
	}
}

static void alt_sensor_init(void)
{
	lid_uses_bma422 = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_0)));

	base_uses_bmi323 = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_0)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);
