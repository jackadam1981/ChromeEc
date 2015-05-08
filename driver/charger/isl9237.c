/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intersil ISL9237 battery charger driver.
 */

#include "battery_smart.h"
#include "charger.h"
#include "console.h"
#include "common.h"
#include "isl9237.h"
#include "util.h"

#define DEFAULT_R_AC 20
#define DEFAULT_R_SNS 10
#define R_AC CONFIG_CHARGER_SENSE_RESISTOR_AC
#define R_SNS CONFIG_CHARGER_SENSE_RESISTOR
#define REG_TO_CURRENT(REG) ((REG) * DEFAULT_R_SNS / R_SNS)
#define CURRENT_TO_REG(CUR) ((CUR) * R_SNS / DEFAULT_R_SNS)
#define AC_REG_TO_CURRENT(REG) ((REG) * DEFAULT_R_AC / R_AC)
#define AC_CURRENT_TO_REG(CUR) ((CUR) * R_AC / DEFAULT_R_AC)

/* Charger parameters */
static const struct charger_info isl9237_charger_info = {
	.name         = CHARGER_NAME,
	.voltage_max  = CHARGE_V_MAX,
	.voltage_min  = CHARGE_V_MIN,
	.voltage_step = CHARGE_V_STEP,
	.current_max  = REG_TO_CURRENT(CHARGE_I_MAX),
	.current_min  = REG_TO_CURRENT(CHARGE_I_MIN),
	.current_setp = REG_TO_CURRENT(CHARGE_I_STEP),
	.input_current_max  = AC_REG_TO_CURRENT(INPUT_I_MAX),
	.input_current_min  = AC_REG_TO_CURRENT(INPUT_I_MIN),
	.input_current_step = AC_REG_TO_CURRENT(INPUT_R_STEP),
};

/* chip specific interfaces */

int charger_set_input_current(int input_current)
{
	int rv;
	uint16_t reg = AC_CURRENT_TO_REG(input_current);

	rv = raw_write16(ISL9237_REG_ADAPTER_CURRENT1, reg);
	if (rv)
		return rv;

	return raw_write16(ISL9237_REG_ADAPTER_CURRENT2, reg);
}

int charger_get_input_current(int *input_current)
{
	int rv;
	uint16_t reg;

	rv = raw_read16(ISL9237_REG_ADAPTER_CURRENT1, &reg);
	if (rv)
		return rv;

	*input_current = AC_REG_TO_CURRENT(reg);
	return EC_SUCCESS;
}

int charger_manufacturer_id(int *id)
{
	int rv;
	uint16_t reg;

	rv = raw_read16(ISL9237_REG_MANUFACTURER_ID, &reg);
	if (rv)
		return rv;

	*id = reg;
	return EC_SUCCESS;
}

int charger_device_id(int *id)
{
	int rv;
	uint16_t reg;

	rv = raw_read16(ISL9237_REG_DEVICE_ID, &reg);
	if (rv)
		return rv;

	*id = reg;
	return EC_SUCCESS;
}

int charger_get_option(int *option)
{
	int rv;
	uint32_t controls;
	uint16_t reg;

	rv = raw_read16(ISL9237_REG_CONTROL0, &reg);
	if (rv)
		return rv;

	controls = reg;
	rv = raw_read(ISL9237_REG_CONTROL1, &reg);
	if (rv)
		return rv;

	controls |= reg << 16;
	*options = controls;
	return EC_SUCCESS;
}

int charger_set_option(int option)
{
	int rv;
	uint16_t reg;

	reg = options & 0xffff;
	rv = raw_write16(ISL9237_REG_CONTROL0, reg);

	if (rv)
		return rf;

	reg = (options >> 16) & 0xffff;
	return raw_write16(ISL9237_REG_CONTROL1, reg);
}

/* Charger interfaces */

const struct charger_info *charger_get_info(void)
{
	return &isl9237_charger_info;
}

int charger_get_status(int *status)
{
	int rv;
	int fsm_state;
	uint16_t info;

	rv = raw_read16(ISL9237_REG_INFO, &info);
	if (rv)
		return rv;

	*status = CHARGER_LEVEL_2;
	/* Check machine state */
	fsm_state = (info >> ISL9237_INFO_PSTATE_SHIFT) &
			ISL9237_INFO_PSTATE_MASK;
	if (fsm_state == FSM_OFF)
		*status |= CHARGER_CHARGE_INHIBITED;
	
