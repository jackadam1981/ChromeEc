/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "common.h"
#include "console.h"
#include "cros_cbi.h"
#include "driver/accelgyro_bmi3xx.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "mkbp_input_devices.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "power/mt8186.h"
#include "power_button.h"
#include "tablet_mode.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

/* Vol-up key matrix for clamshell */
#define VOL_UP_CLAMSHELL_KEY_ROW 1
#define VOL_UP_CLAMSHELL_KEY_COL 5
/* Vol-up key matrix for convertible */
#define VOL_UP_CONVERTIBLE_KEY_ROW 2
#define VOL_UP_CONVERTIBLE_KEY_COL 9

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

test_export_static bool board_is_clamshell;

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
		set_vol_up_key(VOL_UP_CLAMSHELL_KEY_ROW,
			       VOL_UP_CLAMSHELL_KEY_COL);
	}
	if (val == CONVERTIBLE)
		set_vol_up_key(VOL_UP_CONVERTIBLE_KEY_ROW,
			       VOL_UP_CONVERTIBLE_KEY_COL);
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
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_1)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);

static bool powerbtn_is_enable;

static void board_tablet_mode_change(void)
{
	if (tablet_get_mode()) {
		/* avoid pressing the power button when switching to tablet mode
		 */
		if (power_button_is_pressed()) {
			mkbp_button_update(KEYBOARD_BUTTON_POWER, 0);
			disable_chipset_force_shutdown_button();
		}
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_power_button));
		powerbtn_is_enable = 0;
		CPRINTS("powerbtn is disable!");
	} else {
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_power_button));
		keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);
		powerbtn_is_enable = 1;
		CPRINTS("powerbtn is enable!");
	}
}
DECLARE_HOOK(HOOK_INIT, board_tablet_mode_change, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, board_tablet_mode_change,
	     HOOK_PRIO_DEFAULT);

static void enable_powerbtn_interrupt(void)
{
	if (!powerbtn_is_enable) {
		gpio_enable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_power_button));
		keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);
		powerbtn_is_enable = 1;
		CPRINTS("powerbtn is enable!");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, enable_powerbtn_interrupt,
	     HOOK_PRIO_DEFAULT);

static void disbale_powerbtn_interrupt(void)
{
	if (tablet_get_mode()) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_power_button));
		powerbtn_is_enable = 0;
		CPRINTS("powerbtn is disable!");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME_INIT, disbale_powerbtn_interrupt,
	     HOOK_PRIO_DEFAULT);
