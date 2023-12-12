/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Board re-init for Anraggar board.
 * Anraggar has convertible and clamshell config,
 * and shares the same firmware.
 * So some functions should be disabled for clamshell.
 */
#include "accelgyro.h"
#include "battery.h"
#include "chipset.h"
#include "console.h"
#include "cros_cbi.h"
#include "driver/accel_bma4xx.h"
#include "driver/accel_lis2dw12_public.h"
#include "driver/accelgyro_bmi323.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#define CPRINTS(format, args...) cprints(CC_MOTION_SENSE, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_MOTION_SENSE, format, ##args)

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
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_imu_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_acc_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
	}
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static bool base_use_alt_sensor;
static bool lid_use_alt_sensor;

void motion_interrupt(enum gpio_signal signal)
{
	if (base_use_alt_sensor) {
		lsm6dsm_interrupt(signal);
	} else {
		bmi3xx_interrupt(signal);
	}
}

void lid_accel_interrupt(enum gpio_signal signal)
{
	if (lid_use_alt_sensor) {
		lis2dw12_interrupt(signal);
	} else {
		bma4xx_interrupt(signal);
	}
}

static void alt_sensor_init(void)
{
	base_use_alt_sensor = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_lsm6dsm)));
	lid_use_alt_sensor = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_lis2dw12)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);

static void motionsense_suspend(void)
{
	if (!board_is_clamshell) {
		CPRINTS("--suspend dis");
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_imu));
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, motionsense_suspend, HOOK_PRIO_DEFAULT);

static void motionsense_resume(void)
{
	if (!board_is_clamshell) {
		CPRINTS("--resume en");
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_imu));
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_imu));
		motion_interrupt(0);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, motionsense_resume, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, motionsense_resume,
	     MOTION_SENSE_HOOK_PRIO - 1);

int board_sensor_at_360(void)
{
	if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) { /* S0ix/S3 */
		CPRINTS("--360 hall force resume");
		hook_notify(HOOK_CHIPSET_RESUME);
		host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
	}

	return !gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_tablet_mode_l));
}

enum battery_present battery_hw_present(void)
{
	const struct gpio_dt_spec *batt_pres;

	batt_pres = GPIO_DT_FROM_NODELABEL(gpio_ec_battery_pres_odl);

	/* The GPIO is low when the battery is physically present */
	return gpio_pin_get_dt(batt_pres) ? BP_NO : BP_YES;
}
