/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "gpio/gpio_int.h"
#include "hooks.h"
#include "keyboard_scan.h"
#include "math_util.h"
#include "mkbp_input_devices.h"
#include "power.h"
#include "power/mt8186.h"
#include "power_button.h"
#include "tablet_mode.h"
#include "util.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <dt-bindings/battery.h>

#define VOL_UP_KEY_ROW 0
#define VOL_UP_KEY_COL 11

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

static void board_setup_init(void)
{
	set_vol_up_key(VOL_UP_KEY_ROW, VOL_UP_KEY_COL);
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);

static enum battery_present cached_batt_state = BP_NO;

/*
 * I2C read register to detect battery
 */
static void update_battery_state_cache(void)
{
	int state;

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

static bool powerbtn_is_enabale;

static void board_tablet_mode_change(void)
{
	enum power_state chipset_state = power_get_state();
	/* disable powerbutton interrupt when tablet mode change on s0 */
	if (chipset_state == POWER_S0) {
		if (tablet_get_mode()) {
			if (power_button_is_pressed()) {
				mkbp_button_update(KEYBOARD_BUTTON_POWER, 0);
				disable_chipset_force_shutdown_botton();
			}
			gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_power_button));
			powerbtn_is_enabale = 0;
			CPRINTS("powerbtn is disable!");
		} else {
			gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_power_button));
			keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);
			powerbtn_is_enabale = 1;
			CPRINTS("powerbtn is enable!");
		}
	}
}
DECLARE_HOOK(HOOK_INIT, board_tablet_mode_change, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, board_tablet_mode_change,
	     HOOK_PRIO_DEFAULT);

static void enable_powerbtn_interrupt(void)
{
	if (!powerbtn_is_enabale){
		gpio_enable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_power_button));
		keyboard_scan_enable(1, KB_SCAN_DISABLE_POWER_BUTTON);
		powerbtn_is_enabale = 1;
		CPRINTS("powerbtn is enable!");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, enable_powerbtn_interrupt,
	     HOOK_PRIO_DEFAULT);

static void disbale_powerbtn_interrupt(void)
{
	if (tablet_get_mode()) {
		if (power_button_is_pressed()) {
			mkbp_button_update(KEYBOARD_BUTTON_POWER, 0);
			disable_chipset_force_shutdown_botton();
		}
		gpio_disable_dt_interrupt(GPIO_INT_FROM_NODELABEL(int_power_button));
		powerbtn_is_enabale = 0;
		CPRINTS("powerbtn is disable!");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME_INIT, disbale_powerbtn_interrupt,
	     HOOK_PRIO_DEFAULT);
