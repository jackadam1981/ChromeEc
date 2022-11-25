/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charger.h"
#include "common.h"
#include "console.h"
#include "battery.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"
#include "usb_pd.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include "gpio/gpio_int.h"
#include <zephyr/drivers/gpio.h>

#define CPRINTS(format, args...) cprints(CC_HOOK, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_HOOK, format, ##args)

static void board_update_motion_sensor_config(void)
{
	motion_sensor_count = 0;
	gmr_tablet_switch_disable();
	gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_accel_gyro));
	gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_accel_gyro_int_l),
			      GPIO_INPUT | GPIO_PULL_DOWN);
}

static void board_init(void)
{
	board_update_motion_sensor_config();
}

DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_PRE_DEFAULT);
