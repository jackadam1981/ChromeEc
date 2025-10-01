/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "cros_cbi.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motionsense_sensors.h"
#include "tablet_mode.h"

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
	cros_cbi_ssfc_check_match(
		CBI_SSFC_VALUE_ID(DT_NODELABEL(base_sensor_bmi323)));

	motion_sensors_check_ssfc();
}
DECLARE_HOOK(HOOK_INIT, alt_sensor_init, HOOK_PRIO_POST_I2C);
