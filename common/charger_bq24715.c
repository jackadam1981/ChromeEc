/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq24715 battery charger driver.
 */

#include "charger.h"
#include "charger_bq24715.h"
#include "console.h"
#include "common.h"
#include "smart_battery.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
/* Note: it is assumed that the sense resistors are 10mOhm. */

static const struct charger_info bq24725_charger_info = {
	.name         = "bq24715",
	.voltage_max  = CHARGE_V_MAX,
	.voltage_min  = CHARGE_V_MIN,
	.voltage_step = CHARGE_V_STEP,
	.current_max  = CHARGE_I_MAX,
	.current_min  = CHARGE_I_MIN,
	.current_step = CHARGE_I_STEP,
	.input_current_max  = INPUT_I_MAX,
	.input_current_min  = INPUT_I_MIN,
	.input_current_step = INPUT_I_STEP,
};

static int cached_voltage;

int charger_set_input_current(int input_current)
{
	return sbc_write(BQ24715_INPUT_CURRENT, input_current);
}

int charger_get_input_current(int *input_current)
{
	int rv;
	int reg;

	rv = sbc_read(BQ24715_INPUT_CURRENT, &reg);
	if (rv)
		return rv;

	*input_current = reg;

	return EC_SUCCESS;
}

int charger_manufacturer_id(int *id)
{
	return sbc_read(BQ24715_MANUFACTURER_ID, id);
}

int charger_device_id(int *id)
{
	return sbc_read(BQ24715_DEVICE_ID, id);
}

int charger_get_option(int *option)
{
	return sbc_read(BQ24715_CHARGE_OPTION, option);
}

int charger_set_option(int option)
{
	return sbc_write(BQ24715_CHARGE_OPTION, option);
}

/* Charger interfaces */

const struct charger_info *charger_get_info(void)
{
	return &bq24725_charger_info;
}

int charger_get_status(int *status)
{
	int rv;
	int option;

	rv = charger_get_option(&option);
	if (rv)
		return rv;

	/* Default status */
	*status = CHARGER_LEVEL_2;

	if ((option & OPT_CHARGE_INHIBIT_MASK) == OPT_CHARGE_DISABLE)
		*status |= CHARGER_CHARGE_INHIBITED;

	return EC_SUCCESS;
}

int charger_set_mode(int mode)
{
	int rv;
	int option;

	rv = charger_get_option(&option);
	if (rv)
		return rv;

	option &= ~OPT_CHARGE_INHIBIT_MASK;
	if (mode & CHARGE_FLAG_INHIBIT_CHARGE) {
		option |= OPT_CHARGE_DISABLE;
	}
	else
		option |= OPT_CHARGE_ENABLE;
	return charger_set_option(option);
}

int charger_get_current(int *current)
{
	int rv;
	int reg;

	rv = sbc_read(SB_CHARGING_CURRENT, &reg);
	if (rv)
		return rv;

	*current = reg;
	return EC_SUCCESS;
}

int charger_closest_current(int current)
{
	const struct charger_info * const info = charger_get_info();

	/*
	 * If the requested current is non-zero but below our minimum,
	 * return the minimum.  See crosbug.com/p/8662.
	 */
	if (current > 0 && current < info->current_min)
		return info->current_min;

	/* Clip to max */
	if (current > info->current_max)
		return info->current_max;

	/* Otherwise round down to nearest current step */
	return current - (current % info->current_step);
}

int charger_set_current(int current)
{
	current = charger_closest_current(current);

	CPRINTF("[%T setting chrg I to %d]\n", current);
	return sbc_write(SB_CHARGING_CURRENT, current);
}

int charger_get_voltage(int *voltage)
{
	int ret;
	if (cached_voltage == 0) {
		*voltage = cached_voltage;
		return EC_SUCCESS;
	}
	ret = sbc_read(SB_CHARGING_VOLTAGE, &cached_voltage);
	if (ret == EC_SUCCESS)
		*voltage = cached_voltage;
	return ret;
}

int charger_set_voltage(int voltage)
{
	CPRINTF("[%T setting chrg V to %d]\n", voltage);
	cached_voltage = voltage;
	return sbc_write(SB_CHARGING_VOLTAGE, voltage);
}

/* Charging power state initialization */
int charger_post_init(void)
{
	/* Set charger input current limit */
	return charger_set_input_current(CONFIG_CHARGER_INPUT_CURRENT);
}
