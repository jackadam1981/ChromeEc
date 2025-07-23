/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "common.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/accel_bma422.h"
#include "driver/accelgyro_bmi260.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(kaladin_sensor, LOG_LEVEL_INF);

static int sensor_fwconfig;

void motion_interrupt(enum gpio_signal signal)
{
	bmi260_interrupt(signal);
}

void lid_accel_interrupt(enum gpio_signal signal)
{
	bma4xx_interrupt(signal);
}

static void motionsense_init(void)
{
	int ret;

	ret = cros_cbi_get_fw_config(FW_TABLET, &sensor_fwconfig);
	if (ret < 0) {
		LOG_ERR("error retriving CBI config: %d", ret);
		return;
	}
}
DECLARE_HOOK(HOOK_INIT, motionsense_init, HOOK_PRIO_DEFAULT);
