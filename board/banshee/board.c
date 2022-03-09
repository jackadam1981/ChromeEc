/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "button.h"
#include "charge_ramp.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "driver/accel_lis2dw12.h"
#include "driver/accelgyro_lsm6dso.h"
#include "driver/als_tcs3400.h"
#include "driver/charger/isl9241.h"
#include "fw_config.h"
#include "hooks.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "tablet_mode.h"
#include "throttle_ap.h"
#include "usbc_config.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

void board_set_charger_current_limit_deferred(void)
{
	int chgnum = 0;
	int ctl3_val;
	int rv;

	rv = i2c_read16(chg_chips[chgnum].i2c_port,
				chg_chips[chgnum].i2c_addr_flags,
				ISL9241_REG_CONTROL3, &ctl3_val);
	if (rv)
		CPRINTF("Could not get charger input current limit! Error: %d\n"
		, rv);

	if (extpower_is_present() &&
		(battery_get_disconnect_state() != BATTERY_NOT_DISCONNECTED))
		/* AC only or AC+DC but battery is disconnect */
		ctl3_val |= ISL9241_CONTROL3_INPUT_CURRENT_LIMIT;
	else
		ctl3_val &= ~ISL9241_CONTROL3_INPUT_CURRENT_LIMIT;

	rv = i2c_write16(chg_chips[chgnum].i2c_port,
				chg_chips[chgnum].i2c_addr_flags,
				ISL9241_REG_CONTROL3, ctl3_val);

	if (rv)
		CPRINTF("Could not set charger input current limit! Error: %d\n"
		, rv);
}

DECLARE_DEFERRED(board_set_charger_current_limit_deferred);
DECLARE_HOOK(HOOK_SECOND, board_set_charger_current_limit_deferred,
	HOOK_PRIO_DEFAULT);

void battery_present_interrupt(enum gpio_signal signal)
{
	hook_call_deferred(&board_set_charger_current_limit_deferred_data, 0);
}

void board_init(void)
{
	gpio_enable_interrupt(GPIO_EC_BATT_PRES_ODL);
	hook_call_deferred(&board_set_charger_current_limit_deferred_data, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
