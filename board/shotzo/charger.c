/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Shotzo has a charger but has no battery.
 * Add fake battery subfunctions for charger functional requirements.
 * Add a fake battery info for charger to set Vsys.
 */

#include "charge_state.h"
#include "console.h"
#include "hooks.h"

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

enum ec_error_list charger_get_vbus_voltage(int port, int *voltage)
{
	int chgnum = port;

	if (chgnum)
		return EC_ERROR_INVAL;

	if (!chg_chips[chgnum].drv->get_vbus_voltage)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->get_vbus_voltage(chgnum, port, voltage);
}

enum ec_error_list charger_set_otg_current_voltage(int chgnum,
						   int output_current,
						   int output_voltage)
{
	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return EC_ERROR_INVAL;
	}

	if (!chg_chips[chgnum].drv->set_otg_current_voltage)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->set_otg_current_voltage(
		chgnum, output_current, output_voltage);
}

int charger_is_sourcing_otg_power(int port)
{
	int chgnum = port;

	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return 0;
	}

	if (!chg_chips[chgnum].drv->is_sourcing_otg_power)
		return 0;

	return chg_chips[chgnum].drv->is_sourcing_otg_power(chgnum, port);
}

enum ec_error_list charger_enable_otg_power(int chgnum, int enabled)
{
	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return EC_ERROR_INVAL;
	}

	if (!chg_chips[chgnum].drv->enable_otg_power)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->enable_otg_power(chgnum, enabled);
}

enum ec_error_list charger_device_id(int *id)
{
	int chgnum = 0;

	if (!chg_chips[chgnum].drv->device_id)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->device_id(chgnum, id);
}

enum ec_error_list charger_get_voltage(int chgnum, int *voltage)
{
	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return EC_ERROR_INVAL;
	}

	if (!chg_chips[chgnum].drv->get_voltage)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->get_voltage(chgnum, voltage);
}

enum ec_error_list charger_get_current(int chgnum, int *current)
{
	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return EC_ERROR_INVAL;
	}

	if (!chg_chips[chgnum].drv->get_current)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->get_current(chgnum, current);
}

static void charger_chips_init(void)
{
	int chip = 0;

	if (chg_chips[chip].drv->init)
		chg_chips[chip].drv->init(chip);
}
DECLARE_HOOK(HOOK_INIT, charger_chips_init, HOOK_PRIO_POST_I2C);

enum ec_error_list charger_set_input_current_limit(int chgnum,
						   int input_current)
{
	/* Note: may be called with CHARGE_PORT_NONE regularly */
	if (chgnum) {
		CPRINTS("%s(%d) Invalid charger!", __func__, chgnum);
		return EC_ERROR_INVAL;
	}

	if (!chg_chips[chgnum].drv->set_input_current_limit)
		return EC_ERROR_UNIMPLEMENTED;

	return chg_chips[chgnum].drv->set_input_current_limit(chgnum,
							      input_current);
}

/*
 * The board_get_charger_voltage_min() will be called by the sm5803.c
 * to get the min VSYS/VBAT.
 */
int board_get_charger_voltage_min(void)
{
	return 8400; /* mV */
}

/*
 * The board_get_charger_voltage_max() will be called by the sm5803.c
 * to get the max VSYS/VBAT.
 */
int board_get_charger_voltage_max(void)
{
	return 8600; /* mV */
}
