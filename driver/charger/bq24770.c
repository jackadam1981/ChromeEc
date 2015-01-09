/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq24770 battery charger driver.
 */

#include "battery_smart.h"
#include "bq24770.h"
#include "charger.h"
#include "i2c.h"

/* Sense resistor configurations and macros */
#define DEFAULT_SENSE_RESISTOR 10
#define R_SNS CONFIG_CHARGER_SENSE_RESISTOR
#define R_AC  (CONFIG_CHARGER_SENSE_RESISTOR_AC)
#define REG_TO_CURRENT(REG, RS) ((REG) * DEFAULT_SENSE_RESISTOR / (RS))
#define CURRENT_TO_REG(CUR, RS) ((CUR) * (RS) / DEFAULT_SENSE_RESISTOR)
#define REG8_TO_CURRENT(REG, RS) ((REG) * DEFAULT_SENSE_RESISTOR / (RS) )
#define CURRENT_TO_REG8(CUR, RS) ((CUR) * (RS) / DEFAULT_SENSE_RESISTOR )

/* Charger parameters */
static const struct charger_info bq24770_charger_info = {
	.name         = "bq24770",
	.voltage_max  = CHARGE_V_MAX,
	.voltage_min  = CHARGE_V_MIN,
	.voltage_step = CHARGE_V_STEP,
	.current_max  = REG_TO_CURRENT(CHARGE_I_MAX, R_SNS),
	.current_min  = REG_TO_CURRENT(CHARGE_I_MIN, R_SNS),
	.current_step = REG_TO_CURRENT(CHARGE_I_STEP, R_SNS),
	.input_current_max  = REG_TO_CURRENT(INPUT_I_MAX, R_AC),
	.input_current_min  = REG_TO_CURRENT(INPUT_I_MIN, R_AC),
	.input_current_step = REG_TO_CURRENT(INPUT_I_STEP, R_AC),
};

inline static int bq24770_read(int offset, int *data)
{
	return i2c_read16(I2C_PORT_CHARGER, BQ24770_ADDR, offset, data);
}

inline static int bq24770_write(int offset, int data)
{
	return i2c_write16(I2C_PORT_CHARGER, BQ24770_ADDR, offset, data);
}

/* bq24770 specific interfaces */

int charger_set_input_current(int input_current)
{
	return bq24770_write(BQ24770_INPUT_CURRENT, CURRENT_TO_REG8(input_current, R_AC));
}

int charger_get_input_current(int *input_current)
{
	int rv;
	int reg;

	rv = bq24770_read(BQ24770_INPUT_CURRENT, &reg);
	if (rv)
		return rv;

	*input_current = REG8_TO_CURRENT(reg, R_AC);

	return EC_SUCCESS;
}

int charger_manufacturer_id(int *id)
{
	return bq24770_read(BQ24770_MANUFACTURER_ID, id);
}

int charger_device_id(int *id)
{
	return bq24770_read(BQ24770_DEVICE_ADDRESS, id);
}

int charger_get_option(int *option)
{
	return bq24770_read(BQ24770_CHARGE_OPTION0, option);
}

int charger_set_option(int option)
{
	return bq24770_write(BQ24770_CHARGE_OPTION0, option);
}

int charger_get_option1(int *option)
{
	return bq24770_read(BQ24770_CHARGE_OPTION1, option);
}

int charger_set_option1(int option)
{
	return bq24770_write(BQ24770_CHARGE_OPTION1, option);
}

int charger_get_prochot_option0(int *option)
{
	return bq24770_read(BQ24770_PROCHOT_OPTION0, option);
}

int charger_set_prochot_option0(int option)
{
	return bq24770_write(BQ24770_PROCHOT_OPTION0, option);
}

int charger_get_prochot_option1(int *option)
{
	return bq24770_read(BQ24770_PROCHOT_OPTION1, option);
}

int charger_set_prochot_option1(int option)
{
	return bq24770_write(BQ24770_PROCHOT_OPTION1, option);
}

int charger_get_minimun_charge_voltage(int *voltage)
{
	return bq24770_read(BQ24770_MIN_SYSTEM_VOLTAGE, voltage);
}

int charger_set_mininum_system_voltage(int voltage)
{
	return bq24770_write(BQ24770_MIN_SYSTEM_VOLTAGE, voltage);
}

/* Charger interfaces */

const struct charger_info *charger_get_info(void)
{
	return &bq24770_charger_info;
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

	if (option & OPTION0_CHARGE_INHIBIT)
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

	if (mode & CHARGE_FLAG_INHIBIT_CHARGE)
		option |= OPTION0_CHARGE_INHIBIT;
	else
		option &= ~OPTION0_CHARGE_INHIBIT;

	return charger_set_option(option);
}

int charger_get_current(int *current)
{
	int rv;
	int reg;

	rv = bq24770_read(BQ24770_CHARGE_CURRENT, &reg);
	if (rv)
		return rv;

	*current = REG_TO_CURRENT(reg, R_SNS);
	return EC_SUCCESS;
}

int charger_set_current(int current)
{
	current = charger_closest_current(current);

	return bq24770_write(BQ24770_CHARGE_CURRENT, CURRENT_TO_REG(current, R_SNS));
}

int charger_get_voltage(int *voltage)
{
	return bq24770_read(BQ24770_MAX_CHARGE_VOLTAGE, voltage);
}

int charger_set_voltage(int voltage)
{
	return bq24770_write(BQ24770_MAX_CHARGE_VOLTAGE, voltage);
}

/* Charging power state initialization */
int charger_post_init(void)
{
        int rv, option;

	rv = charger_get_option(&option);
	if (rv)
		return rv;

	option &= ~OPTION0_LOW_POWER_MODE_EN;
	option |= OPTION0_ACOC_SETTING;
	option &= ~OPTION0_LSFET_OCP_THRESHOLD;

	rv = charger_set_option(option);
	if (rv)
		return rv;

	rv = charger_get_option1(&option);
	if (rv)
		return rv;

	option &= ~OPTION1_AUTO_WAKEUP_EN;

	rv = charger_set_option1(option);
	if (rv)
		return rv;

	rv = charger_get_prochot_option0(&option);
	if (rv)
		return rv;

	option &= ~PROCHOT_OPTION0_ICRIT_COMPARATOR_THRESHOLD_MASK;
	option |= PROCHOT_OPTION0_ICRIT_COMPARATOR_THRESHOLD_150_PERCENTAGE;

	option &= ~PROCHOT_OPTION0_VSYS_COMPARATOR_THRESHOLD_MASK;
	option |= PROCHOT_OPTION0_VSYS_COMPARATOR_THRESHOLD_3P35_V;

	option |= PROCHOT_OPTION0_PULSE_EXTENSION_ENABLE;

	option &= ~PROCHOT_OPTION0_PULSE_WIDTH_MASK;
	option |= PROCHOT_OPTION0_PULSE_WIDTH_1_MS;

	option &= ~PROCHOT_OPTION0_HOST_CLEAR_IDLE;

	option |= PROCHOT_OPTION0_INOM_COMPARATOR_DEGLITCH_50_MS;

	rv = charger_set_prochot_option0(option);
	if (rv)
		return rv;

	rv = charger_get_prochot_option1(&option);
	if (rv)
		return rv;

	option &= ~PROCHOT_OPTION1_IDCHG_COMPARATOR_THRESHOLD_MASK;
	option |= PROCHOT_OPTION1_IDCHG_COMPARATOR_THRESHOLD_4096_MA;

	option &= ~PROCHOT_OPTION1_IDCHG_COMPARATOR_DEGLITCH_TIME_MASK;
	option |= PROCHOT_OPTION1_IDCHG_COMPARATOR_DEGLITCH_TIME_1P6_MS;

	option &= ~PROCHOT_OPTION1_PROCHOT_ENVELOP_SELECTOR_MASK;
	option |= ENVELOP_SELECTOR_ICRIT_ENABLE;
	option |= ENVELOP_SELECTOR_INOM_ENABLE;
	option |= ENVELOP_SELECTOR_IDCHG_ENABLE;
	option |= ENVELOP_SELECTOR_VSYS_ENABLE;

	rv = charger_set_prochot_option1(option);
	if (rv)
		return rv;

	/* Set minimum charge voltage: 3325 mV */
	charger_set_mininum_system_voltage(0xD00);

	return EC_SUCCESS;
}

int charger_discharge_on_ac(int enable)
{
	int rv;
	int option;

	rv = charger_get_option(&option);
	if (rv)
		return rv;

	if (enable)
		rv = charger_set_option(option | OPTION0_LEARN_ENABLE);
	else
		rv = charger_set_option(option & ~OPTION0_LEARN_ENABLE);

	return rv;
}

