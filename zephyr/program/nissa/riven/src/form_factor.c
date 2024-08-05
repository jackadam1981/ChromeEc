/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_board_info.h"
#include "cros_cbi.h"
#include "driver/accel_bma4xx.h"
#include "driver/accel_bma5xy.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

enum base_sensor_type {
	base_bma422 = 0,
	base_bma530,
};

enum lid_sensor_type {
	lid_bma422 = 0,
	lid_bma530,
};

static int lid_alt_sensor;
static int base_alt_sensor;

void lid_accel_interrupt(enum gpio_signal signal)
{
	if (lid_alt_sensor == lid_bma530)
		bma5xy_interrupt(signal);
	else
		bma4xx_interrupt(signal);
}

test_export_static void alt_sensor_init(void)
{
	/* check which motion sensors are used */
	if (cros_cbi_ssfc_check_match(
		    CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_1)))) {
		base_alt_sensor = base_bma530;
		ccprints("BASE ACCEL IS BMA530");
	} else {
		base_alt_sensor = base_bma422;
		ccprints("BASE ACCEL IS BMA422");
	}

	if (cros_cbi_ssfc_check_match(
		    CBI_SSFC_VALUE_ID(DT_NODELABEL(lid_sensor_1)))) {
		lid_alt_sensor = lid_bma530;
		ccprints("LID SENSOR IS BMA530");
	} else {
		lid_alt_sensor = lid_bma422;
		ccprints("LID SENSOR IS BMA422");
	}

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C + 1);

test_export_static void clamshell_init(void)
{
	int ret;
	uint32_t val;

	/* Check if it's clamshell or convertible */
	ret = cros_cbi_get_fw_config(FORM_FACTOR, &val);
	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d", FORM_FACTOR);
		return;
	}
	if (val == CLAMSHELL) {
		LOG_INF("Clamshell: disable motionsense function.");
		motion_sensor_count = 0;
		gmr_tablet_switch_disable();
		gpio_disable_dt_interrupt(
			GPIO_INT_FROM_NODELABEL(int_lid_accel));
		gpio_pin_configure_dt(GPIO_DT_FROM_NODELABEL(gpio_acc_int_l),
				      GPIO_INPUT | GPIO_PULL_UP);
	}
}
DECLARE_HOOK(HOOK_INIT, clamshell_init, HOOK_PRIO_POST_DEFAULT);
