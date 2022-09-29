/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "accelgyro.h"
#include "driver/accelgyro_bmi3xx.h"
#include "motionsense_sensors.h"

void motion_interrupt(enum gpio_signal signal)
{
	bmi3xx_interrupt(signal);
}
