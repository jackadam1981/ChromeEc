/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "common.h"
#include "cros_cbi.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

static bool base_use_alt_sensor;

void motion_interrupt(enum gpio_signal signal)
{
	if (base_use_alt_sensor) {
		lsm6dsm_interrupt(signal);
	} else {
		bmi3xx_interrupt(signal);
	}
}

static void alt_sensor_init(void)
{
	base_use_alt_sensor = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_1)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);

static void gpiom2_s0(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_m2), 1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, gpiom2_s0, HOOK_PRIO_DEFAULT);

static void gpiom2_s3(void)
{
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_m2), 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, gpiom2_s3, HOOK_PRIO_DEFAULT);