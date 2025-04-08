/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "common.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "math_util.h"
#include "util.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/battery.h>

#define VOL_UP_KEY_ROW 0
#define VOL_UP_KEY_COL 11

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

static void board_setup_init(void)
{
	set_vol_up_key(VOL_UP_KEY_ROW, VOL_UP_KEY_COL);
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static enum battery_present cached_batt_state = BP_NO;

bool board_battery_is_initialized(void)
{
	int batt_status;

	return battery_status(&batt_status) != EC_SUCCESS ?
		       false :
		       !!(batt_status & STATUS_INITIALIZED);
}

/*
 * I2C read register to detect battery
 */
static void update_battery_state_cache(void)
{
	int state;

	/**
	 * Only when the complete battery information is read, the DUT will
	 * perform bad battery detection. If there is a communication
	 * abnormality, it will be judged as BP_YES.
	 */
	if (!board_battery_is_initialized()) {
		cached_batt_state = BP_YES;
		return;
	}

	/*
	 *  According to the battery manufacturer's reply:
	 *  To detect a bad battery, need to read the 0x00 register.
	 *  If the 12th bit(Permanently Failure) is 1, it means a bad battery.
	 */
	if (sb_read(SB_MANUFACTURER_ACCESS, &state)) {
		cached_batt_state = BP_NO;
		return;
	}

	/* Detect the 12th bit value */
	if (state & BIT(12)) {
		cached_batt_state = BP_NO;
	} else {
		cached_batt_state = BP_YES;
	}
}
/*
 *  i2c read takes 3.5ms. Battery_is_present is called continuously
 *  during startup, which delays the DUT loading powerd. To avoid
 *  the delay of powerd, put the i2c read action into
 *  update_battery_state_cache and call it once every 1S to
 *  record the register status.
 */
DECLARE_HOOK(HOOK_SECOND, update_battery_state_cache, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, update_battery_state_cache, HOOK_PRIO_DEFAULT);

enum battery_present battery_is_present(void)
{
	if (gpio_get_level(GPIO_BATT_PRES_ODL)) {
		return BP_NO;
	}
	return cached_batt_state;
}
