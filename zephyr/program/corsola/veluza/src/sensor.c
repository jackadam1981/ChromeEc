/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_cbi.h"
#include "driver/accel_bma422.h"
#include "driver/accelgyro_bmi260.h"
#include "driver/accelgyro_bmi323.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"
#include "timer.h"
#include "watchdog.h"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Number of elements in an array */
//define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct watchdog_info {
	const struct device *wdt_dev;
	struct wdt_timeout_cfg config;
};

static bool base_use_alt_sensor;

extern const struct watchdog_info wdt_info[];
extern int watchdog_init_device(const struct watchdog_info *info);

struct watchdog_info wdt_info_temp[1/*ARRAY_SIZE(wdt_info)*/];

static void restore_wdt_period(void)
{
	/* Restore WDT period */
	watchdog_init_device(&wdt_info[0]);
}
DECLARE_DEFERRED(restore_wdt_period);

static void restore_wdt_period_in_hook(void)
{
	/* Restore WDT period */
	watchdog_init_device(&wdt_info[0]);

	/* Cancel restore_wdt_period() defer function */
	hook_call_deferred(&restore_wdt_period_data, -1);
}
DECLARE_HOOK(HOOK_TICK, restore_wdt_period_in_hook, HOOK_PRIO_LAST);

static void runtime_increase_wdt_period(void)
{
	memcpy(&wdt_info_temp[0], &wdt_info[0], sizeof(struct watchdog_info)); /*DEVICE_DT_GET()*/
	/* Increase WDT period */
	wdt_info_temp[0].config.window.max = 10 * SECOND;

	/* Install and setup WDT period */
	watchdog_init_device(&wdt_info_temp[0]); //watchdog_config(const struct watchdog_info *info);
						 //watchdog_enable(const struct device *wdt_dev);

	/* Restore WDT period by defer and HOOK_TICK */
	hook_call_deferred(&restore_wdt_period_data, 10 * SECOND);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, runtime_increase_wdt_period, HOOK_PRIO_POST_DEFAULT);

void base_sensor_interrupt(enum gpio_signal signal)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_FORM_FACTOR, &val);
	if (val == FW_FORM_FACTOR_CONVERTIBLE) {
		if (base_use_alt_sensor)
			bmi3xx_interrupt(signal);
		else
			bmi260_interrupt(signal);
	}
}

void lid_sensor_interrupt(enum gpio_signal signal)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_FORM_FACTOR, &val);
	if (val == FW_FORM_FACTOR_CONVERTIBLE)
		bma4xx_interrupt(signal);
}

static void disable_base_lid_irq(void)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_FORM_FACTOR, &val);
	if (val == FW_FORM_FACTOR_CLAMSHELL) {
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_base_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(base_imu_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_lid_imu));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(lid_accel_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
	}
}
DECLARE_HOOK(HOOK_INIT, disable_base_lid_irq, HOOK_PRIO_POST_DEFAULT);

static void board_sensor_init(void)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_FORM_FACTOR, &val);
	if (val == FW_FORM_FACTOR_CLAMSHELL) {
		ccprints("Board is Clamshell");
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
	} else if (val == FW_FORM_FACTOR_CONVERTIBLE) {
		ccprints("Board is Convertible");
	}
}
DECLARE_HOOK(HOOK_INIT, board_sensor_init, HOOK_PRIO_DEFAULT);

static void alt_sensor_init(void)
{
	base_use_alt_sensor = cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_bmi323)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);
