/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_state.h"
#include "charger.h"
#include "console.h"
#include "driver/charger/bq25710.h"
#include "driver/charger/bq257x0_regs.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ##args)

static void set_bq25720_charge_option_2(void)
{
	int rv;
	int chgnum = charge_get_active_chg_chip();
	int reg = 0x0037;

	rv = bq257x0_set_option_reg(chgnum, 0x31, reg);

	if (rv)
		CPRINTF("Failed to set CHARGE_OPTION_2 (rv=%d)\n", rv);
	else
		CPRINTF("CHARGE_OPTION_2 set to 0x%04x\n", reg);
}

static void set_bq25720_charge_option_4(void)
{
	int rv;
	int chgnum = charge_get_active_chg_chip();
	int reg = 0x0040;

	rv = bq257x0_set_option_reg(chgnum, 0x36, reg);

	if (rv)
		CPRINTF("Failed to set CHARGE_OPTION_4 (rv=%d)\n", rv);
	else
		CPRINTF("CHARGE_OPTION_4 set to 0x%04x\n", reg);
}

static void set_bq25720_vmin_active(void)
{
	int rv;
	int chgnum = charge_get_active_chg_chip();
	int reg = 0x00fc;

	rv = bq257x0_set_option_reg(chgnum, 0x37, reg);

	if (rv)
		CPRINTF("Failed to set Vmim_Active (rv=%d)\n", rv);
	else
		CPRINTF("Vmim_Active set to 0x%04x\n", reg);
}

static void set_bq25720_min_system_voltage(void)
{
	int rv;
	int chgnum = charge_get_active_chg_chip();
	int reg = 0x7800;

	rv = bq257x0_set_option_reg(chgnum, 0x3e, reg);

	if (rv)
		CPRINTF("Failed to set Min_system_voltage (rv=%d)\n", rv);
	else
		CPRINTF("Min_system_voltage set to 0x%04x\n", reg);
}

static void set_bq25720_input_voltage_limit(void)
{
	int chgnum = charge_get_active_chg_chip();
	int rv;
	int reg_value = 0x0240;
	int reg_addr = BQ25710_REG_INPUT_VOLTAGE;

	rv = bq257x0_set_option_reg(chgnum, reg_addr, reg_value);
	if (rv)
		CPRINTF("Failed to set INPUT_VOLTAGE_LIMIT (rv=%d)\n", rv);
	else
		CPRINTF("Set INPUT_VOLTAGE_LIMIT (0x3D) = 0x%04x\n", reg_value);
}
static void set_chg_reg_custom(void)
{
	set_bq25720_charge_option_2();
	set_bq25720_charge_option_4();
	set_bq25720_vmin_active();
	set_bq25720_min_system_voltage();
	set_bq25720_input_voltage_limit();
}
DECLARE_HOOK(HOOK_INIT, set_chg_reg_custom, HOOK_PRIO_POST_BATTERY_INIT + 1);
