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
#define STABLE_THRESHOLD 3
#define BAD_DELAY (500 * USEC_PER_MSEC)
#define GOOD_DELAY (120 * 1000 * USEC_PER_MSEC)

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

static void board_setup_init(void)
{
	set_vol_up_key(VOL_UP_KEY_ROW, VOL_UP_KEY_COL);
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static enum battery_present cached_batt_state = BP_NO;
static enum battery_present raw_state = BP_NO;
static int stable_count;
/*
 * I2C read register to detect battery
 */
static void update_battery_state_cache(void)
{
	int state;
	int rv;
	enum battery_present current;

	/*
	 *  According to the battery manufacturer's reply:
	 *  To detect a bad battery, need to read the 0x00 register.
	 *  If the 12th bit(Permanently Failure) is 1, it means a bad battery.
	 */
	rv = sb_read(SB_MANUFACTURER_ACCESS, &state);

	if (rv || (state & BIT(12)))
		current = BP_NO;
	else
		current = BP_YES;

	if (current == raw_state) {
		if (stable_count < STABLE_THRESHOLD)
			stable_count++;
	} else {
		raw_state = current;
		stable_count = 1;
	}

	if (stable_count >= STABLE_THRESHOLD)
		cached_batt_state = raw_state;

	hook_call_deferred(&update_battery_state_cache_data,
			   (cached_batt_state == BP_NO) ? BAD_DELAY :
							  GOOD_DELAY);
}
/*
 *  i2c read takes 3.5ms. Battery_is_present is called continuously
 *  during startup, which delays the DUT loading powerd. To avoid
 *  the delay of powerd, put the i2c read action into
 *  update_battery_state_cache and call it once every 1S to
 *  record the register status.
 */
DECLARE_DEFERRED(update_battery_state_cache);
DECLARE_HOOK(HOOK_INIT, update_battery_state_cache, HOOK_PRIO_DEFAULT);

enum battery_present battery_is_present(void)
{
	if (gpio_get_level(GPIO_BATT_PRES_ODL)) {
		return BP_NO;
	}
	return cached_batt_state;
}
