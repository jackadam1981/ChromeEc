/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI bq24773 battery charger driver.
 */

#include "battery_smart.h"
#include "bq24773.h"
#include "charger.h"
#include "console.h"
#include "common.h"
#include "util.h"

/*
 * on the I2C version of the charger,
 * some registers are 8-bit only (eg input current)
 * and they are shifted by 6 bits compared to the SMBUS version (bq24770).
 */
#define REG8_SHIFT 6
#define R8 (1 << (REG8_SHIFT))
/* Sense resistor configurations and macros */
#define DEFAULT_SENSE_RESISTOR 10
#define R_SNS CONFIG_CHARGER_SENSE_RESISTOR
#define R_AC  (CONFIG_CHARGER_SENSE_RESISTOR_AC)
#define REG_TO_CURRENT(REG, RS) ((REG) * DEFAULT_SENSE_RESISTOR / (RS))
#define CURRENT_TO_REG(CUR, RS) ((CUR) * (RS) / DEFAULT_SENSE_RESISTOR)
#define REG8_TO_CURRENT(REG, RS) ((REG) * DEFAULT_SENSE_RESISTOR / (RS) * R8)
#define CURRENT_TO_REG8(CUR, RS) ((CUR) * (RS) / DEFAULT_SENSE_RESISTOR / R8)

/* Charger parameters */
static const struct charger_info bq2477x_charger_info = {
	.name         = CHARGER_NAME,
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

/* chip specific interfaces */

int charger_set_input_current(int input_current)
{
#ifdef CONFIG_CHARGER_BQ24770
	return raw_write16(REG_INPUT_CURRENT, CURRENT_TO_REG(input_current, R_AC));
#elif defined(CONFIG_CHARGER_BQ24773)
	return raw_write8(REG_INPUT_CURRENT, CURRENT_TO_REG8(input_current, R_AC));
#endif
}

int charger_get_input_current(int *input_current)
{
	int rv;
	int reg;

#ifdef CONFIG_CHARGER_BQ24770
	rv = raw_read16(REG_INPUT_CURRENT, &reg);
#elif defined(CONFIG_CHARGER_BQ24773)
	rv = raw_read8(REG_INPUT_CURRENT, &reg);
#endif
	if (rv)
		return rv;

#ifdef CONFIG_CHARGER_BQ24770
	*input_current = REG8_TO_CURRENT(reg, R_AC);
#elif defined(CONFIG_CHARGER_BQ24773)
	*input_current = REG_TO_CURRENT(reg, R_AC);
#endif
	return EC_SUCCESS;
}

int charger_manufacturer_id(int *id)
{
#ifdef CONFIG_CHARGER_BQ24770
	return raw_read16(REG_MANUFACTURE_ID, id);
#elif defined(CONFIG_CHARGER_BQ24773)
	*id = 0x40; /* TI */
	return EC_SUCCESS;
#endif
}

int charger_device_id(int *id)
{
#ifdef CONFIG_CHARGER_BQ24770
	return raw_read16(REG_DEVICE_ADDRESS, id);
#elif defined(CONFIG_CHARGER_BQ24773)
	return raw_read8(REG_DEVICE_ADDRESS, id);
#endif
}

int charger_get_option(int *option)
{
	return raw_read16(REG_CHARGE_OPTION0, option);
}

int charger_set_option(int option)
{
	return raw_write16(REG_CHARGE_OPTION0, option);
}

/* Charger interfaces */

const struct charger_info *charger_get_info(void)
{
	return &bq2477x_charger_info;
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

	rv = raw_read16(REG_CHARGE_CURRENT, &reg);

	if (rv)
		return rv;

	*current = REG_TO_CURRENT(reg, R_SNS);
	return EC_SUCCESS;
}

int charger_set_current(int current)
{
	current = charger_closest_current(current);
	return raw_write16(REG_CHARGE_CURRENT, CURRENT_TO_REG(current, R_SNS));
}

int charger_get_voltage(int *voltage)
{
	return raw_read16(REG_MAX_CHARGE_VOLTAGE, voltage);
}

int charger_set_voltage(int voltage)
{
	voltage = charger_closest_voltage(voltage);
	return raw_write16(REG_MAX_CHARGE_VOLTAGE, voltage);
}

#if defined(CONFIG_CHARGER_LOW_POWER_MODE_DISABLE)            || \
    defined(CONFIG_CHARGER_ACOC_LIMIT_300_PERCENTAGE_OF_IDPM) || \
    defined(CONFIG_CHARGER_LSFET_OCP_THRESHOLD_290MV)
#define SET_CHARGE_OPTION0
static int charger_post_init_set_charge_option0(void)
{
	int rv, option;

	rv = charger_get_option(&option);
	if (rv)
		return rv;

/* ChargeOption[15]: Low power mode enable, POR = 1 */
#ifdef CONFIG_CHARGER_LOW_POWER_MODE_DISABLE
	option &= ~OPTION0_LOW_POWER_MODE_ENABLE;
#endif

/* ChargeOption[7]: ACOC Setting, POR = 0 */
#ifdef CONFIG_CHARGER_ACOC_LIMIT_300_PERCENTAGE_OF_IDPM
	option |= OPTION0_ACOC_SETTING;
#endif

/* ChargeOption[6]: LSFET OCP Threshold, POR = 0 (170mV) */
#ifdef CONFIG_CHARGER_LSFET_OCP_THRESHOLD_290MV
	option |= OPTION0_LSFET_OCP_THRESHOLD;
#endif

	return charger_set_option(option);
}
#endif

#if defined(CONFIG_CHARGER_AUTO_WAKEUP_DISABLE)
#define SET_CHARGE_OPTION1
static int charger_post_init_set_charge_option1(void)
{
	int rv, option1;

	rv = raw_read16(REG_CHARGE_OPTION1, &option1);
	if (rv)
		return rv;

/* ChargeOption0[0]: Auto Wakeup Enable, POR = 1 */
#ifdef CONFIG_CHARGER_AUTO_WAKEUP_DISABLE
	option1 &= ~OPTION1_AUTO_WAKEUP_ENABLE;
#endif

	return raw_write16(REG_CHARGE_OPTION1, option1);
}
#endif

#if defined(CONFIG_CHARGER_ILIM_PIN_DISABLED)
#define SET_CHARER_OPTION2
static int charger_post_init_set_charge_option2(void)
{
	int rv, option2;

	rv = raw_read16(REG_CHARGE_OPTION2, &option2);
	if (rv)
		return rv;

#ifdef CONFIG_CHARGER_ILIM_PIN_DISABLED
	option2 &= ~OPTION2_EN_EXTILIM;
#endif

	return raw_write16(REG_CHARGE_OPTION2, option2);
}
#endif

#if defined(CONFIG_CHARGER_ICRIT_COMPARATOR_THRESHOLD_150_PERCENTAGE) || \
    defined(CONFIG_CHARGER_VSYS_COMPARATOR_THRESHOLD_3350_MV)         || \
    defined(CONFIG_CHARGER_PROCHOT_PULSE_EXTENSION_ENABLE)            || \
    defined(CONFIG_CHARGER_PROCHOT_PULSE_WIDTH_1MS)                   || \
    defined(CONFIG_CHARGER_PROCHOT_HOST_CLEAR)                        || \
    defined(CONFIG_CHARGER_INOM_COMPARATOR_DEGLITCH_50MS)
#define SET_PROCHOT_OPTION0
static int charger_post_init_set_prochot_option0(void)
{
	int rv, option;

	rv = raw_read16(REG_PROTECT_OPTION0, &option);
	if (rv)
		return rv;

/* ChargeOption2[15..11]: ICRIT Comparator Threshold */
#ifdef CONFIG_CHARGER_ICRIT_COMPARATOR_THRESHOLD_150_PERCENTAGE
	option &= ~PROCHOT0_ICRIT_COMPARATOR_THRESHOLD_MASK;
	option |= PROCHOT0_ICRIT_COMPARATOR_THRESHOLD_150_PERCENTAGE;
#endif

/* ChargeOption2[7..6]: VSYS comparator threshold, POR = 01 (6V or 3.1V) */
#ifdef CONFIG_CHARGER_VSYS_COMPARATOR_THRESHOLD_3350_MV
	option &= ~PROCHOT0_VSUS_COMPARATOR_THRESHOLD_MASK;
	option |= PROCHOT0_VSUS_COMPARATOR_THRESHOLD_3350_MV;
#endif

/* ChargeOptino2[5]: PROCHOT Pulse Extension Enable, POR = 0 */
#ifdef CONFIG_CHARGER_PROCHOT_PULSE_EXTENSION_ENABLE
	option |= PROCHOT0_PULSE_EXTENSION_ENABLE;
#endif

/* ChargeOption[4..3]: PROCHOT Pulse Width, POR = 10 (12ms) */
#ifdef CONFIG_CHARGER_PROCHOT_PULSE_WIDTH_1MS
	option &= ~PROCHOT0_PULSE_WIDTH_MASK;
	option |= PROCHOT0_PULSE_WIDTH_1MS;
#endif

/* ChargeOption[2]: PROCHOT Host Clear, POR = 1 (Idle) */
#ifdef CONFIG_CHARGER_PROCHOT_HOST_CLEAR
	option &= ~PROCHOT0_HOST_CLEAR;
#endif

/* ChargeOption[1]: INOM Comparator Deglitch Time, POR = 0 (1ms) */
#ifdef CONFIG_CHARGER_INOM_COMPARATOR_DEGLITCH_50MS
	option |= PROCHOT0_INOM_COMPARATOR_DEGLITCH_50MS;
#endif

	return raw_write16(REG_PROTECT_OPTION0, option);
}
#endif

#if defined(CONFIG_CHARGER_IDCHG_COMPARATOR_THRESHOLD_4096_MA) || \
    defined(CONFIG_CHARGER_IDCHG_COMPARATOR_DEGLITCH_1P6_MS)   || \
    defined(CONFIG_CHARGER_ENVELOP_SELECTOR_ICRIT)             || \
    defined(CONFIG_CHARGER_ENVELOP_SELECTOR_INOM)              || \
    defined(CONFIG_CHARGER_ENVELOP_SELECTOR_IDCHG)             || \
    defined(CONFIG_CHARGER_ENVELOP_SELECTOR_VSYS)
#define SET_PROCHOT_OPTION1
static int charger_post_init_set_prochot_option1(void)
{
	int rv, option1;

	rv = raw_read16(REG_PROTECT_OPTION1, &option1);
	if (rv)
		return rv;

/* ProchotOption1[15..10]: IDCHG Comparator Threshold */
#ifdef CONFIG_CHARGER_IDCHG_COMPARATOR_THRESHOLD_4096_MA
	option1 &= ~PROCHOT1_IDCHG_COMPARATOR_THRESHOLD_MASK;
	option1 |= PROCHOT1_IDCHG_COMPARATOR_THRESHOLD_4096_MA;
#endif

/* ProchotOption1[9..8]: IDCHG Comparator Deglitch Time */
#ifdef CONFIG_CHARGER_IDCHG_COMPARATOR_DEGLITCH_1P6_MS
	option1 &= ~PROCHOT1_IDCHG_COMPARATOR_DEGLITCH_MASK;
#endif

/* ProchotOption1[6..0]: PROCHOT envelop selector */
#ifdef CONFIG_CHARGER_ENVELOP_SELECTOR_ICRIT
	option1 |= PROCHOT1_ENVELOP_SELECTOR_ICRIT;
#endif

#ifdef CONFIG_CHARGER_ENVELOP_SELECTOR_INOM
	option1 |= PROCHOT1_ENVELOP_SELECTOR_INOM;
#endif

#ifdef CONFIG_CHARGER_ENVELOP_SELECTOR_IDCHG
	option1 |= PROCHOT1_ENVELOP_SELECTOR_IDCHG;
#endif

#ifdef CONFIG_CHARGER_ENVELOP_SELECTOR_VSYS
	option1 |= PROCHOT1_ENVELOP_SELECTOR_VSYS;
#endif

	return raw_write16(REG_PROTECT_OPTION1, option1);
}
#endif

/* Charging power state initialization */
int charger_post_init(void)
{
	int rv = EC_SUCCESS;

#ifdef SET_CHARGE_OPTION0
	rv = charger_post_init_set_charge_option0();
	if (rv)
		return rv;
#endif

#ifdef SET_CHARGE_OPTION1
	rv = charger_post_init_set_charge_option1();
	if (rv)
		return rv;
#endif

#ifdef SET_CHARER_OPTION2
	rv = charger_post_init_set_charge_option2();
	if (rv)
		return rv;
#endif

#ifdef SET_PROCHOT_OPTION0
	rv = charger_post_init_set_prochot_option0();
	if (rv)
		return rv;
#endif

#ifdef SET_PROCHOT_OPTION1
	rv = charger_post_init_set_prochot_option1();
	if (rv)
		return rv;
#endif

	return rv;
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
