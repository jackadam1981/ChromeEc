/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_cbi.h"
#include "driver/accel_bma422.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_bmi323.h"
#include "driver/accelgyro_icm42607.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

test_export_static bool base_is_none;
test_export_static bool lid_is_none;

void base_sensor_interrupt(enum gpio_signal signal)
{
	base_is_none = true;
}

void lid_sensor_interrupt(enum gpio_signal signal)
{
	lid_is_none = true;
}
