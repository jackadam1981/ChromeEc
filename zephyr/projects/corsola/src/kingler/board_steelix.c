/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board re-init for Rusty board
 * Rusty shares the firmware with Steelix.
 * Steelix is convertible but Rusty is clamshell
 * so some functions should be disabled for clamshell.
 */
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#include "common.h"
#include "accelgyro.h"

#include "cros_cbi.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "driver/accelgyro_bmi3xx.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

static bool board_is_clamshell;

static void board_setup_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FORM_FACTOR, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FORM_FACTOR);
		return;
	}
	if (val == CLAMSHELL) {
		board_is_clamshell = true;
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
	}
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static void disable_base_imu_irq(void)
{
	if (board_is_clamshell) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_base_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(base_imu_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
	}
}
DECLARE_HOOK(HOOK_INIT, disable_base_imu_irq, HOOK_PRIO_POST_DEFAULT);

enum base_sensor {
	BASE_SENSOR_NONE,
	BASE_SENSOR_BMI323,
	BASE_SENSOR_LSM6DSM,
};

enum lid_sensor {
	LID_SENSOR_NONE,
	LID_SENSOR_BMA422,
	LID_SENSOR_LIS2DW12,
};

enum base_sensor get_base_sensor(void)
{
	if (cros_cbi_ssfc_check_match(
		    CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_0)))) {
		return BASE_SENSOR_BMI323;
	} else if (cros_cbi_ssfc_check_match(
			   CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_1)))) {
		return BASE_SENSOR_LSM6DSM;
	} else {
		return BASE_SENSOR_NONE;
	}
}

enum lid_sensor get_lid_sensor(void)
{
	if (cros_cbi_ssfc_check_match(
		    CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_0)))) {
		return LID_SENSOR_BMA422;
	} else if (cros_cbi_ssfc_check_match(
			   CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_1)))) {
		return LID_SENSOR_LIS2DW12;
	} else {
		return LID_SENSOR_NONE;
	}
}

void motion_interrupt(enum gpio_signal signal)
{
	if (get_base_sensor() == BASE_SENSOR_BMI323) {
		bmi3xx_interrupt(signal);
	} else {
		lsm6dsm_interrupt(signal);
	}
}

static void motionsense_init(void)
{
	if (get_base_sensor() == BASE_SENSOR_BMI323) {
		LOG_ERR("BASE ACCEL is BMI323");
	} else if (get_base_sensor() == BASE_SENSOR_LSM6DSM) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_base_accel);
		MOTIONSENSE_ENABLE_ALTERNATE(alt_base_gyro);
		LOG_ERR("BASE ACCEL IS LSM6DSM");
	} else {
		LOG_ERR("no base sensor");
	}

	if (get_lid_sensor() == LID_SENSOR_BMA422) {
		LOG_ERR("BASE ACCEL is BMA422");
	} else if (get_lid_sensor() == LID_SENSOR_LIS2DW12) {
		MOTIONSENSE_ENABLE_ALTERNATE(alt_lid_accel);
		LOG_ERR("BASE ACCEL IS LIS2DW12");
	} else {
		LOG_ERR("no lid sensor");
	}
}
DECLARE_HOOK(HOOK_INIT, motionsense_init, HOOK_PRIO_DEFAULT);
