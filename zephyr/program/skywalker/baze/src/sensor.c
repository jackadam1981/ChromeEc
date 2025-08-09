/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_board_info.h"
#include "driver/accel_bma4xx.h"
#include "driver/accel_lis2dw12_public.h"
#include "hooks.h"
#include "motionsense_sensors.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_sensor, LOG_LEVEL_INF);

void lid_accel_interrupt(enum gpio_signal signal)
{
	bma4xx_interrupt(signal);
}
