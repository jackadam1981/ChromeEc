/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Richtek RT9479 2-4cell NVDC and Bypass Buck-Boost Battery Charger driver.
 */

#include "battery.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "charge_ramp.h"
#include "charge_state.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "rt9479.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#ifndef CONFIG_CHARGER_RT9479
#error Only the RT9479 is supported by rt9479 driver.
#endif

#ifndef CONFIG_CHARGER_NARROW_VDC
#error "rt9479 is a NVDC charger, please enable CONFIG_CHARGER_NARROW_VDC."
#endif

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ##args)

/* Sense resistor configurations and macros */
#define RT9479_DEFAULT_RS_BUS 10 /* Input current sense resistor */
#define RT9479_DEFAULT_RS_BAT 10 /* Battery charge current sense resistor */

/* RT9479 DAC PARAMETER */
#define RT9479_AICR1_MAX 8400
#define RT9479_AICR1_MIN 50
#define RT9479_AICR1_STEP (125 / 10)

#define RT9479_DISCHARGE_CURRENT1_MAX 32256
#define RT9479_DISCHARGE_CURRENT1_MIN 0
#define RT9479_DISCHARGE_CURRENT1_STEP 512

#define RT9479_CHARGE_CURRENT_MAX 8124
#define RT9479_CHARGE_CURRENT_MIN 0
#define RT9479_CHARGE_CURRENT_STEP 64

#define RT9479_CHARGE_VOLTAGE_MAX 19200
#define RT9479_CHARGE_VOLTAGE_MIN 5000
#define RT9479_CHARGE_VOLTAGE_STEP 8

#define RT9479_MIVR_MAX 19520
#define RT9479_MIVR_MIN 3904
#define RT9479_MIVR_STEP 64

#define RT9479_VBUSOK_MAX 19875
#define RT9479_VBUSOK_MIN 4000
#define RT9479_VBUSOK_STEP 125

#define CHARGE_V_MAX RT9479_CHARGE_VOLTAGE_STEP
#define CHARGE_V_STEP RT9479_CHARGE_VOLTAGE_STEP
#define CHARGE_I_STEP RT9479_CHARGE_CURRENT_STEP
#define INPUT_I_STEP RT9479_AICR1_STEP

#define RS_BUS CONFIG_CHARGER_SENSE_RESISTOR_AC
#define RS_BAT CONFIG_CHARGER_SENSE_RESISTOR

#define REG_TO_CURRENT(reg, rs_default, rs) ((reg) * rs_default / (rs))
#define CURRENT_TO_REG(cur, rs_default, rs) ((cur) * (rs) / rs_default)

/* RT9479 DAC */
#define REG_IBUS_TO_CURRENT(reg)                                          \
	(uint16_t)((REG_TO_CURRENT(reg, RT9479_DEFAULT_RS_BUS, RS_BUS)) * \
		   INPUT_I_STEP)
#define CURRENT_TO_REG_IBUS(cur)                                          \
	(uint16_t)((CURRENT_TO_REG(cur, RT9479_DEFAULT_RS_BUS, RS_BUS)) / \
		   INPUT_I_STEP)

#define REG_IBAT_TO_CURRENT(reg)                                          \
	(uint16_t)((REG_TO_CURRENT(reg, RT9479_DEFAULT_RS_BAT, RS_BAT)) * \
		   CHARGE_I_STEP)
#define CURRENT_TO_REG_IBAT(cur)                                          \
	(uint16_t)((CURRENT_TO_REG(cur, RT9479_DEFAULT_RS_BAT, RS_BAT)) / \
		   CHARGE_I_STEP)

#define REG_VBAT_TO_VOLTAGE(reg) (reg * CHARGE_V_STEP)
#define VOLTAGE_TO_REG_VBAT(reg) (reg / CHARGE_V_STEP)

/* RT9479 ADC */
#define REG_ADC_TO_VALUE(reg, lsb, msb, step) \
	(REG_FIELD_GET(reg, lsb, msb) * step)

#define REG_ADC_VBAT_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 0, 7, step)
#define REG_ADC_VSYS_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 8, 15, step)
#define REG_ADC_IBAT_CHG_TO_CURRENT(reg, step) \
	REG_ADC_TO_VALUE(reg, 8, 14, step)
#define REG_ADC_IBAT_DCHG_TO_CURRENT(reg, step) \
	REG_ADC_TO_VALUE(reg, 0, 6, step)
#define REG_ADC_IBUS_TO_CURRENT(reg, step) REG_ADC_TO_VALUE(reg, 8, 15, step)
#define REG_ADC_PSYS_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 0, 7, step)
#define REG_ADC_VBUS_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 8, 15, step)
#define REG_ADC_NTC_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 6, 15, step)
#define REG_ADC_GP_TO_VOLTAGE(reg, step) REG_ADC_TO_VALUE(reg, 8, 15, step)

enum adc_mode {
	ADC_MODE_ONESHOT = 0,
	ADC_MODE_CONTINUE = 1,
};

enum en_adc {
	EN_ADC_VBAT,
	EN_ADC_VSYS,
	EN_ADC_IBAT_CHG,
	EN_ADC_IBAT_DCHG,
	EN_ADC_IBUS,
	EN_ADC_PSYS,
	EN_ADC_VBUS,
	EN_ADC_NTC_GP,
};

#define _ADC_CHAN(en_adc) ((1 << en_adc) << RT9479_EN_ADC_SHIFT)

enum adc_channel {
	ADC_VBAT = _ADC_CHAN(EN_ADC_VBAT),
	ADC_VSYS = _ADC_CHAN(EN_ADC_VSYS),
	ADC_IBAT_CHG = _ADC_CHAN(EN_ADC_IBAT_CHG),
	ADC_IBAT_DCHG = _ADC_CHAN(EN_ADC_IBAT_DCHG),
	ADC_IBUS = _ADC_CHAN(EN_ADC_IBUS),
	ADC_PSYS = _ADC_CHAN(EN_ADC_PSYS),
	ADC_VBUS = _ADC_CHAN(EN_ADC_VBUS),
	ADC_NTC_GP = _ADC_CHAN(EN_ADC_NTC_GP),
};

#define RT9479_ADC_CONV_TIME_MS \
	(5 * 6) // one channel transfer time, temp 5ms for 6 channel
#define RT9479_BYPASS_NVDC_TRANSITION_TIME_MS (500)

#define RT9479_MV_TO_VBUSOK_REG(mv) \
	(((mv - RT9479_VBUSOK_MIN) / RT9479_VBUSOK_STEP) << 8)
#define RT9479_MV_TO_MIVR_REG(mv) \
	(((mv - RT9479_MIVR_MIN) / RT9479_MIVR_STEP) << 4)

static int learn_mode;

/* Charger information */
static const struct charger_info rt9479_charger_info = {
	.name = "rt9479",
	.voltage_max = RT9479_CHARGE_VOLTAGE_MAX,
	.voltage_min = RT9479_CHARGE_VOLTAGE_MIN,
	.voltage_step = RT9479_CHARGE_VOLTAGE_STEP,
	.current_max = REG_IBAT_TO_CURRENT(RT9479_CHARGE_CURRENT_MAX),
	.current_min = REG_IBAT_TO_CURRENT(RT9479_CHARGE_CURRENT_MIN),
	.current_step = REG_IBAT_TO_CURRENT(RT9479_CHARGE_CURRENT_STEP),
	.input_current_max = REG_IBUS_TO_CURRENT(RT9479_AICR1_MAX),
	.input_current_min = REG_IBUS_TO_CURRENT(RT9479_AICR1_MIN),
	.input_current_step = REG_IBUS_TO_CURRENT(RT9479_AICR1_STEP),
};

/* Mutex for CONTROL1 register, that can be updated from multiple tasks. */
static K_MUTEX_DEFINE(control1_mutex_rt9479);
/* Mutex for CONTROL3 register, that can be updated from multiple tasks. */
static K_MUTEX_DEFINE(control3_mutex_rt9479);

static enum ec_error_list rt9479_discharge_on_ac(int chgnum, int enable);
static enum ec_error_list rt9479_discharge_on_ac_unsafe(int chgnum, int enable);
static enum ec_error_list rt9479_discharge_on_ac_weak_disable(int chgnum);

static inline enum ec_error_list rt9479_read16(int chgnum, int offset,
					       int *value)
{
	int rv = i2c_read16(chg_chips[chgnum].i2c_port,
			    chg_chips[chgnum].i2c_addr_flags, offset, value);
	if (rv)
		CPRINTS("%s failed (%d)", __func__, rv);

	return rv;
}

static inline enum ec_error_list rt9479_write16(int chgnum, int offset,
						int value)
{
	int rv = i2c_write16(chg_chips[chgnum].i2c_port,
			     chg_chips[chgnum].i2c_addr_flags, offset, value);
	if (rv)
		CPRINTS("%s failed (%d)", __func__, rv);

	return rv;
}

static inline enum ec_error_list rt9479_update16(int chgnum, int offset,
						 uint16_t mask,
						 enum mask_update_action action)
{
	int rv = i2c_update16(chg_chips[chgnum].i2c_port,
			      chg_chips[chgnum].i2c_addr_flags, offset, mask,
			      action);
	if (rv)
		CPRINTS("%s failed (%d)", __func__, rv);

	return rv;
}

static inline enum ec_error_list rt9479_field_update16(int chgnum, int offset,
						       uint16_t field_mask,
						       const uint16_t set_value)
{
	int rv = i2c_field_update16(chg_chips[chgnum].i2c_port,
				    chg_chips[chgnum].i2c_addr_flags, offset,
				    field_mask, set_value);
	if (rv)
		CPRINTS("%s failed (%d)", __func__, rv);

	return rv;
}

static int rt9479_set_adc_conv_mode(int chgnum, int mode)
{
	enum mask_update_action action =
		(mode == ADC_MODE_CONTINUE) ? MASK_SET : MASK_CLR;
	int rv;

	rv = rt9479_update16(chgnum, RT9479_REG_ADCOPTION, RT9479_ADC_CONV_MODE,
			     action);

	return rv;
}

__maybe_unused static int rt9479_get_adc_conv_mode(int chgnum, int *mode)
{
	int rv, reg;

	rv = rt9479_read16(chgnum, RT9479_REG_ADCOPTION, &reg);
	if (rv)
		return rv;

	*mode = reg & RT9479_ADC_CONV_MODE;

	return rv;
}

static int rt9479_set_adc_conv_start(int chgnum, bool enable)
{
	enum mask_update_action action = enable ? MASK_SET : MASK_CLR;
	int rv;

	rv = rt9479_update16(chgnum, RT9479_REG_CONTROL3, RT9479_ADC_START,
			     action);

	return rv;
}

static int rt9479_set_adc_conv_channel(int chgnum, int channel, bool enable)
{
	int rv;

	rv = rt9479_field_update16(chgnum, RT9479_REG_ADCOPTION, channel,
				   enable ? channel : 0);

	return rv;
}

static int _rt9479_enable_adc_conv_oneshot(int chgnum, int channel)
{
	/* todo: temp only supply one shot */
	int rv, reg;
	timestamp_t deadline;

	/* disable adc */
	rv = rt9479_set_adc_conv_start(chgnum, false);
	if (rv)
		return rv;

	/* check in one shot mode*/
	rv = rt9479_set_adc_conv_mode(chgnum, ADC_MODE_ONESHOT);
	if (rv)
		return rv;

	/* set conv channel */
	rv = rt9479_set_adc_conv_channel(chgnum, channel, true);
	if (rv)
		return rv;

	/* enable adc */
	rv = rt9479_set_adc_conv_start(chgnum, false);
	if (rv)
		return rv;

	/* adc finish or timeout */
	deadline.val = get_time().val + RT9479_ADC_CONV_TIME_MS * MSEC;
	do {
		rv = rt9479_read16(chgnum, RT9479_REG_CONTROL3, &reg);
		if (rv)
			return rv;

		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
	} while (!!(reg & RT9479_ADC_START));

	/* disable conv channel */
	rv = rt9479_set_adc_conv_channel(chgnum, channel, false);

	return rv;
}

static int rt9479_get_adc_scale(int chgnum, int channel, int *scale)
{
	int rv, reg;

	switch (channel) {
	case ADC_VBAT:
		*scale = 64; // mv
		break;
	case ADC_VSYS:
		*scale = 64; // mv
		break;
	case ADC_IBAT_CHG:
		rv = rt9479_read16(chgnum, RT9479_REG_CHARGEOPTION1, &reg);
		*scale = REG_FIELD_GET(reg, 13, 13) ? 128 : 64; // mA
		break;
	case ADC_IBAT_DCHG:
		rv = rt9479_read16(chgnum, RT9479_REG_CHARGEOPTION1, &reg);
		*scale = REG_FIELD_GET(reg, 13, 13) ? 512 : 256; // mA
		break;
	case ADC_IBUS:
		rv = rt9479_read16(chgnum, RT9479_REG_CHARGEOPTION1, &reg);
		*scale = REG_FIELD_GET(reg, 12, 13) ? 100 : 50; // mA
		break;
	case ADC_PSYS:
		rv = rt9479_read16(chgnum, RT9479_REG_ADCOPTION, &reg);
		*scale = REG_FIELD_GET(reg, 14, 14) ? 12 : 8; // mV
		break;
	case ADC_VBUS:
		*scale = 96; // mV
		break;
	case ADC_NTC_GP:
		rv = rt9479_read16(chgnum, RT9479_REG_ADCOPTION, &reg);
		*scale = REG_FIELD_GET(reg, 14, 14) ? 12 : 8; // mV
		break;
	default:
		rv = EC_ERROR_UNKNOWN;
		break;
	}
	return rv;
}

static int rt9479_transfer_adc_value(int chgnum, int channel, int scale,
				     int *val)
{
	int rv, reg;
	bool ntc;

	switch (channel) {
	case ADC_VBAT:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_VBAT, &reg);
		*val = REG_ADC_VBAT_TO_VOLTAGE(reg, scale);
		break;
	case ADC_VSYS:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_VSYS, &reg);
		*val = REG_ADC_VSYS_TO_VOLTAGE(reg, scale);
		break;
	case ADC_IBAT_CHG:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_IBAT_CHG, &reg);
		*val = REG_ADC_IBAT_CHG_TO_CURRENT(reg, scale);
		break;
	case ADC_IBAT_DCHG:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_IBAT_DISCHG, &reg);
		*val = REG_ADC_IBAT_DCHG_TO_CURRENT(reg, scale);
		break;
	case ADC_IBUS:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_IBUS, &reg);
		*val = REG_ADC_IBUS_TO_CURRENT(reg, scale);
		break;
	case ADC_PSYS:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_VBUSPSYS, &reg);
		*val = REG_ADC_PSYS_TO_VOLTAGE(reg, scale);
		break;
	case ADC_VBUS:
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_VBUSPSYS, &reg);
		*val = REG_ADC_VBUS_TO_VOLTAGE(reg, scale);
		break;
	case ADC_NTC_GP:
		rv = rt9479_read16(chgnum, RT9479_REG_CONTROL4, &reg);
		if (rv)
			return rv;
		ntc = REG_FIELD_GET(reg, 14, 14) ? 1 : 0;
		rv = rt9479_read16(chgnum, RT9479_REG_ADC_NTC_GP, &reg);
		if (ntc) {
			*val = REG_ADC_NTC_TO_VOLTAGE(reg, scale);
		} else {
			*val = REG_ADC_GP_TO_VOLTAGE(reg, scale);
		}
		break;
	default:
		rv = EC_ERROR_UNKNOWN;
		break;
	}
	return rv;
}

static int rt9479_enable_adc_oneshot(int chgnum, int channel)
{
	int rv;

	mutex_lock(&control3_mutex_rt9479);
	rv = _rt9479_enable_adc_conv_oneshot(chgnum, channel);
	if (rv)
		CPRINTS("%s adc conv failed (%d)", __func__, rv);

	mutex_unlock(&control3_mutex_rt9479);
	return rv;
}

static int rt9479_get_adc_value(int chgnum, int channel, int *val)
{
	int rv;
	int scale;

	mutex_lock(&control3_mutex_rt9479);

	rv = rt9479_get_adc_scale(chgnum, channel, &scale);
	if (rv) {
		mutex_unlock(&control3_mutex_rt9479);
		CPRINTS("%s get adc scale failed (%d)", __func__, rv);
		return rv;
	}

	rv = rt9479_transfer_adc_value(chgnum, channel, scale, val);
	if (rv)
		CPRINTS("%s get adc failed (%d)", __func__, rv);

	mutex_unlock(&control3_mutex_rt9479);
	return rv;
}

static enum ec_error_list rt9479_set_input_current_limit(int chgnum,
							 int input_current)
{
	int rv;
	/* AICR2 = AICR1 * Ratio */
	uint16_t reg = CURRENT_TO_REG_IBUS(input_current);

	rv = rt9479_field_update16(chgnum, RT9479_REG_IAICR1,
				   RT9479_REG_IAICR1_MASK,
				   reg << RT9479_REG_IAICR1_SHIFT);

	return rv;
}

static enum ec_error_list rt9479_set_input_current_limit_2(int chgnum,
							   int ratio)
{
	return rt9479_write16(chgnum, RT9479_REG_AICR2, ratio);
}

static enum ec_error_list rt9479_get_input_current_limit(int chgnum,
							 int *input_current)
{
	int rv;

	rv = rt9479_read16(chgnum, RT9479_REG_IAICR1, input_current);
	if (rv)
		return rv;

	*input_current = REG_FIELD_GET(*input_current, 4, 13);
	*input_current = REG_IBUS_TO_CURRENT(*input_current);
	return EC_SUCCESS;
}

__maybe_unused static int rt9479_get_input_current_limit_2(int chgnum,
							   int *input_current)
{
	int rv, ratio;

	rv = rt9479_read16(chgnum, RT9479_REG_AICR2, &ratio);
	if (rv)
		return rv;

	ratio = GET_AICR2_PERCENTAGE(ratio);
	if (ratio < 110 || ratio > 450)
		return EC_ERROR_PARAM1;

	rv = rt9479_get_input_current_limit(chgnum, input_current);
	if (rv)
		return rv;

	*input_current = ((*input_current) * ratio / 100);

	return EC_SUCCESS;
}

__maybe_unused static int rt9479_set_ibat_dischg_limit(int chgnum,
						       int ibat_dischg)
{
	int rv;
	uint16_t reg = CURRENT_TO_REG_IBAT(ibat_dischg);

	rv = rt9479_field_update16(chgnum, RT9479_REG_IDCHG_TH,
				   RT9479_IDCHG_TH1_MASK,
				   reg << RT9479_IDCHG_TH1_SHIFT);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

__maybe_unused static int rt9479_get_ibat_dischg_limit(int chgnum,
						       int *ibat_dischg)
{
	int rv;

	rv = rt9479_read16(chgnum, RT9479_REG_IDCHG_TH, ibat_dischg);
	if (rv)
		return rv;

	*ibat_dischg = REG_FIELD_GET(*ibat_dischg, 9, 15);
	*ibat_dischg = REG_IBAT_TO_CURRENT(*ibat_dischg);

	return EC_SUCCESS;
}

static enum ec_error_list rt9479_manufacturer_id(int chgnum, int *id)
{
	return rt9479_read16(chgnum, RT9479_REG_MANUFACTURER_ID, id);
}

static enum ec_error_list rt9479_device_id(int chgnum, int *id)
{
	return rt9479_read16(chgnum, RT9479_REG_DEVICE_ID, id);
}

static enum ec_error_list rt9479_set_frequency(int chgnum, int freq_khz)
{
	int rv;
	int freq;

	mutex_lock(&control1_mutex_rt9479);
	/*
	 * 0: 1200kHz
	 * 1: 800kHz (Default)
	 */
	if (freq_khz >= 1000)
		freq = RT9479_PWM_FREQ_1200KHZ;
	else
		freq = RT9479_PWM_FREQ_800KHZ;

	rv = rt9479_field_update16(chgnum, RT9479_REG_CONTROL1, RT9479_PWM_FREQ,
				   freq << RT9479_PWM_FREQ_SHIFT);

	if (rv) {
		CPRINTS("Could not write CONTROL1. (rv=%d)", rv);
		goto error;
	}

error:
	mutex_unlock(&control1_mutex_rt9479);

	return rv;
}

static enum ec_error_list rt9479_get_option(int chgnum, int *option)
{
	int rv;
	uint32_t controls;
	int reg;

	rv = rt9479_read16(chgnum, RT9479_REG_CONTROL0, &reg);
	if (rv)
		return rv;

	controls = reg;
	rv = rt9479_read16(chgnum, RT9479_REG_CONTROL1, &reg);
	if (rv)
		return rv;

	controls |= reg << 16;
	*option = controls;
	return EC_SUCCESS;
}

static enum ec_error_list rt9479_set_option(int chgnum, int option)
{
	int rv;

	rv = rt9479_write16(chgnum, RT9479_REG_CONTROL0, option & 0xFFFF);
	if (rv)
		return rv;

	return rt9479_write16(chgnum, RT9479_REG_CONTROL1,
			      (option >> 16) & 0xFFFF);
}

static const struct charger_info *rt9479_get_info(int chgnum)
{
	return &rt9479_charger_info;
}

static enum ec_error_list rt9479_bypass_mode_enabled(int chgnum, int *enabled)
{
	int reg, rv;

	rv = rt9479_read16(chgnum, RT9479_REG_CONTROL0, &reg);
	if (rv)
		return rv;

	*enabled = !!(reg & RT9479_BYPFET_ON);

	return EC_SUCCESS;
}

static int rt9479_get_battery_present(int chgnum)
{
	int reg, rv;

	rv = rt9479_update16(chgnum, RT9479_REG_CONTROL4, RT9479_PP_BATGONE,
			     MASK_SET);

	if (rv)
		return false;
	rv = rt9479_read16(chgnum, RT9479_REG_PROCHOTSTATUS, &reg);

	return (!!(reg & RT9479_BATGONE_PROC_STAT));
}

static enum ec_error_list rt9479_get_status(int chgnum, int *status)
{
	int rv;
	int reg;

	/* Level 2 charger */
	*status = CHARGER_LEVEL_2;

	/* Charge inhibit status */
	rv = rt9479_read16(chgnum, RT9479_REG_VSYS_MIN, &reg);
	if (rv)
		return rv;
	if (!reg)
		*status |= CHARGER_CHARGE_INHIBITED;

	/* Battery present */
	if (rt9479_get_battery_present(chgnum))
		*status |= CHARGER_BATTERY_PRESENT;

	/* VBUS present status */
	rv = rt9479_read16(chgnum, RT9479_REG_INTERRUPT_STATUS, &reg);
	if (rv)
		return rv;
	if (reg & RT9479_VBUSOK_STAT)
		*status |= CHARGER_AC_PRESENT;

	/* Bypass mode status */
	rv = rt9479_bypass_mode_enabled(chgnum, &reg);
	if (rv)
		return rv;
	if (reg)
		*status |= CHARGER_BYPASS_MODE;

	return EC_SUCCESS;
}

static enum ec_error_list rt9479_set_mode(int chgnum, int mode)
{
	int rv;

	/* need to check
	 * See crosbug.com/p/51196.
	 * Disable learn mode if it wasn't explicitly enabled.
	 */
	rv = rt9479_discharge_on_ac_weak_disable(chgnum);
	if (rv)
		return rv;

	/*
	 * Charger inhibit
	 * MinSystemVoltage 0x00h = disables all battery charging
	 */
	rv = rt9479_write16(chgnum, RT9479_REG_VSYS_MIN,
			    mode & CHARGE_FLAG_INHIBIT_CHARGE ?
				    0 :
				    battery_get_info()->voltage_min);
	if (rv)
		return rv;

	/* POR reset */
	if (mode & CHARGE_FLAG_POR_RESET) {
		mutex_lock(&control3_mutex_rt9479);
		rv = rt9479_write16(chgnum, RT9479_REG_CONTROL3,
				    RT9479_RESET_REG);
		mutex_unlock(&control3_mutex_rt9479);
	}

	return rv;
}

static enum ec_error_list rt9479_get_current(int chgnum, int *current)
{
	int rv;

	rv = rt9479_read16(chgnum, RT9479_REG_CHARGECURRENT, current);
	if (rv)
		return rv;

	*current = REG_FIELD_GET(*current, 6, 12);
	*current = REG_IBAT_TO_CURRENT(*current);
	return EC_SUCCESS;
}

static enum ec_error_list rt9479_set_current(int chgnum, int current)
{
	return rt9479_field_update16(
		chgnum, RT9479_REG_CHARGECURRENT, RT9479_REG_CHARGECURRENT_MASK,
		CURRENT_TO_REG_IBAT(current) << RT9479_REG_CHARGECURRENT_SHIFT);
}

static enum ec_error_list rt9479_get_voltage(int chgnum, int *voltage)
{
	int rv;
	rv = rt9479_read16(chgnum, RT9479_REG_CHARGEVOLTAGE, voltage);

	*voltage = REG_FIELD_GET(*voltage, 3, 14);
	*voltage = REG_VBAT_TO_VOLTAGE(*voltage);
	return EC_SUCCESS;
}

static enum ec_error_list rt9479_set_voltage(int chgnum, int voltage)
{
	return rt9479_field_update16(
		chgnum, RT9479_REG_CHARGEVOLTAGE, RT9479_REG_CHARGEVOLTAGE_MASK,
		VOLTAGE_TO_REG_VBAT(voltage) << RT9479_REG_CHARGEVOLTAGE_SHIFT);
}

static enum ec_error_list rt9479_get_vbus_voltage(int chgnum, int port,
						  int *voltage)
{
	int rv;

	rv = rt9479_enable_adc_oneshot(chgnum, ADC_VBUS);
	rv = rt9479_get_adc_value(chgnum, ADC_VBUS, voltage);

	return rv;
}

static enum ec_error_list rt9479_get_vsys_voltage(int chgnum, int port,
						  int *voltage)
{
	int rv;

	rv = rt9479_enable_adc_oneshot(chgnum, ADC_VSYS);
	rv = rt9479_get_adc_value(chgnum, ADC_VSYS, voltage);

	return rv;
}

static enum ec_error_list rt9479_post_init(int chgnum)
{
	return EC_SUCCESS;
}

/*
 * Writes to RT9479_REG_CONTROL1, unsafe as it does not lock
 * control1_mutex_rt9479.
 */

static enum ec_error_list rt9479_discharge_on_ac_unsafe(int chgnum, int enable)
{
	int rv = rt9479_update16(chgnum, RT9479_REG_CONTROL1,
				 RT9479_CONTROL1_LEARN_MODE,
				 (enable) ? MASK_SET : MASK_CLR);
	if (!rv)
		learn_mode = enable;

	return rv;
}

/* Disables discharge on ac only if it wasn't explicitly enabled. */
static enum ec_error_list rt9479_discharge_on_ac_weak_disable(int chgnum)
{
	int rv = 0;

	mutex_lock(&control1_mutex_rt9479);
	if (!learn_mode) {
		rv = rt9479_discharge_on_ac_unsafe(chgnum, 0);
	}

	mutex_unlock(&control1_mutex_rt9479);
	return rv;
}

static enum ec_error_list rt9479_discharge_on_ac(int chgnum, int enable)
{
	int rv = 0;

	mutex_lock(&control1_mutex_rt9479);
	rv = rt9479_discharge_on_ac_unsafe(chgnum, enable);
	mutex_unlock(&control1_mutex_rt9479);
	return rv;
}

__maybe_unused int rt9479_set_ac_prochot(int chgnum, int ma)
{
	int rv, ratio;
	int reg;

	/*
	 * ac_prochot = inom = ratio * AICR1
	 */

	CPRINTS("set_ac_prochot = (%d)", ma);

	rv = rt9479_read16(chgnum, RT9479_REG_INOM, &reg);
	if (rv)
		return rv;

	ratio = (!!(reg & RT9479_INOM)) ? 105 : 110;

	ma = ma * 100 / ratio;
	if (ma > RT9479_AICR1_MAX)
		ma = RT9479_AICR1_MAX;
	else if (ma < RT9479_AICR1_MIN)
		ma = RT9479_AICR1_MIN;

	rv = rt9479_set_input_current_limit(chgnum, ma);

	if (rv)
		CPRINTS("set_aicr = %d failed (%d)", ma, rv);

	return rv;
}

__maybe_unused int rt9479_set_dc_prochot(int chgnum, int ma)
{
	int rv;

	/*
	 * dc_prochot = idchg_th1
	 */

	CPRINTS("set_dc_prochot = (%d)", ma);

	if (ma > RT9479_DISCHARGE_CURRENT1_MAX)
		ma = RT9479_DISCHARGE_CURRENT1_MAX;
	else if (ma < RT9479_DISCHARGE_CURRENT1_MIN)
		ma = RT9479_DISCHARGE_CURRENT1_MIN;

	rv = rt9479_set_ibat_dischg_limit(chgnum, ma);
	if (rv)
		CPRINTS("set_idchg = %d failed (%d)", ma, rv);

	return rv;
}

#ifdef CONFIG_CHARGER_DUMP_PROCHOT
static int rt9479_get_ac_prochot_inom(int chgnum, int *inom)
{
	int rv, val, ratio;

	rv = rt9479_read16(chgnum, RT9479_REG_INOM, &val);
	if (rv)
		return rv;

	ratio = (!!(val & RT9479_INOM)) ? 105 : 110;

	rv = rt9479_get_input_current_limit(chgnum, &val);
	if (rv)
		return rv;

	*inom = (val * ratio / 100);

	return 0;
}

static int rt9479_get_ac_prochot_icrit(int chgnum, int *icrit)
{
	int rv, val, ratio = 110;

	rv = rt9479_get_input_current_limit_2(chgnum, &val);
	if (rv)
		return rv;

	*icrit = (val * ratio / 100);

	return 0;
}

static int rt9479_get_ac_prochot(int chgnum, int *inom, int *icrit)
{
	/*
	 * INOM: Adapter average current, as 110% of IAICR1
	 * ICRIT: Adapter peak current, as 110% of IAICR2, and IAICR2
	 * 		  is 110% to 450% of IAICR1
	 */
	int rv;
	rv = rt9479_get_ac_prochot_inom(chgnum, inom);
	if (rv)
		return rv;

	rv = rt9479_get_ac_prochot_icrit(chgnum, icrit);

	return rv;
}

static int rt9479_get_dc_prochot(int chgnum, int *idchg, int *idchg2)
{
	/*
	 * IDCHG1: Battery discharge current level 1
	 * IDCHG2: Battery discharge current level 2, 125% to 400% of
	 * 		   IDCHG_TH1
	 */
	int rv, val, ratio;

	rv = rt9479_read16(chgnum, RT9479_REG_IDCHG_TH, &val);
	if (rv)
		return rv;

	*idchg = REG_FIELD_GET(val, 9, 15);

	ratio = GET_IDCHG_TH2_PERCENTAGE(val >> RT9479_IDCHG_TH2_SHIFT);

	*idchg2 = (*idchg) * ratio / 100;

	return 0;
}

static int rt9479_get_prochot_status(int chgnum, bool *out_low_vsys,
				     bool *out_dcprochot, bool *out_acprochot)
{
	int rv;
	int val;

	/* Get prochot statuses. */
	rv = rt9479_read16(chgnum, RT9479_REG_INTERRUPT_STATUS, &val);
	if (rv) {
		CPRINTS("%s: failed to read interrupt status (%d)", __func__,
			rv);
		return rv;
	}
	*out_low_vsys = val & RT9479_VSYS_UVP_STAT;

	rv = rt9479_read16(chgnum, RT9479_REG_PROCHOTSTATUS, &val);
	if (rv) {
		CPRINTS("%s: failed to read prochot status (%d)", __func__, rv);
		return rv;
	}
	*out_dcprochot = !!(val & RT9479_IDCHG1_PROC_STAT);
	*out_dcprochot |= !!(val & RT9479_IDCHG2_PROC_STAT);

	*out_acprochot = !!(val & RT9479_INOM_PROC_STAT);
	*out_acprochot |= !!(val & RT9479_ICRIT_PROC_STAT);

	return rv;
}

__maybe_unused static void rt9479_dump_prochot_status(int chgnum)
{
	bool low_vsys_prochot = false;
	bool dc_prochot = false;
	bool ac_prochot = false;

	int ac_prochot_inom, ac_prochot_icrit;
	int dc_prochot_idchg, dc_prochot_idchg2;

	int vbus, ibus, vbat, ibat_chg, ibat_dchg, vsys;

	int rv = 0;

	rv = rt9479_get_ac_prochot(chgnum, &ac_prochot_inom, &ac_prochot_icrit);
	if (rv) {
		CPRINTS("Failed to get prochot AC limit (%d)", rv);
		return;
	}

	rv = rt9479_get_dc_prochot(chgnum, &dc_prochot_idchg,
				   &dc_prochot_idchg2);
	if (rv) {
		CPRINTS("Failed to get prochot DC limit (%d)", rv);
		return;
	}

	rv = rt9479_get_prochot_status(chgnum, &low_vsys_prochot, &dc_prochot,
				       &ac_prochot);
	if (rv) {
		CPRINTS("Failed to get prochot status (%d)", rv);
		return;
	}

	rv = rt9479_enable_adc_oneshot(
		chgnum, ADC_VBUS | ADC_IBUS | ADC_VBAT | ADC_IBAT_CHG |
				ADC_IBAT_DCHG | ADC_VSYS);

	rt9479_get_adc_value(chgnum, ADC_VBUS, &vbus);
	rt9479_get_adc_value(chgnum, ADC_IBUS, &ibus);
	rt9479_get_adc_value(chgnum, ADC_VBAT, &vbat);
	rt9479_get_adc_value(chgnum, ADC_IBAT_CHG, &ibat_chg);
	rt9479_get_adc_value(chgnum, ADC_IBAT_DCHG, &ibat_dchg);
	rt9479_get_adc_value(chgnum, ADC_VSYS, &vsys);

	CPRINTS("prochot status for charger %d", chgnum);
	CPRINTS("\tProchot status: %s %s %s", low_vsys_prochot ? "LOWVSYS" : "",
		dc_prochot ? "DC" : "", ac_prochot ? "AC" : "");

	CPRINTS("\tDC prochot idischg: %d mA", dc_prochot_idchg);
	CPRINTS("\tDC prochot idischg2: %d mA", dc_prochot_idchg2);
	CPRINTS("\tAC prochot inom: %d mA", ac_prochot_inom);
	CPRINTS("\tAC prochot icrit: %d mA", ac_prochot_icrit);

	CPRINTS("\tVbus: %d mV", vbus);
	CPRINTS("\tIbus: %d mV", ibus);
	CPRINTS("\tVbat: %d mV", vbat);
	CPRINTS("\tIbat_chg: %d mV", ibat_chg);
	CPRINTS("\tIbat_dchg: %d mV", ibat_dchg);
	CPRINTS("\tVsys: %d mV", vsys);
}
#endif /* CONFIG_CHARGER_DUMP_PROCHOT */

#ifdef CONFIG_CHARGER_BYPASS_MODE
static bool rt9479_is_ac_present(int chgnum)
{
	static bool ac_is_present;
	int reg;
	int rv;

	rv = rt9479_read16(chgnum, RT9479_REG_INTERRUPT_STATUS, &reg);
	if (rv == EC_SUCCESS)
		ac_is_present = !!(reg & RT9479_VBUSOK_STAT);

	return ac_is_present;
}

/*
 * Check whether rt9479 is in any CHRG state, including NVDC+CHRG, Bypass+CHRG,
 * RTB+CHRG.
 */
static bool rt9479_is_in_chrg(int chgnum)
{
	static bool trickle_charge_enabled, fast_charge_enabled;
	int reg;
	int rv;

	rv = rt9479_read16(chgnum, RT9479_REG_VSYS_MIN, &reg);
	if (rv == EC_SUCCESS)
		trickle_charge_enabled = reg > 0;

	rv = rt9479_read16(chgnum, RT9479_REG_CHARGECURRENT, &reg);
	if (rv == EC_SUCCESS)
		fast_charge_enabled = reg > 0;

	return trickle_charge_enabled || fast_charge_enabled;
}

/*
 * Transition from NVDC+CHRG to NVDC
 */
static enum ec_error_list rt9479_nvdc_chrg_to_nvdc(int chgnum)
{
	enum ec_error_list rv;

	CPRINTS("nvdc_chrg -> nvdc");
	/* 1: Disable charge. */
	rv = rt9479_set_current(chgnum, 0);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

/*
 * Transition from Bypass + CHRG to Bypass
 */
static enum ec_error_list rt9479_bypass_chrg_to_bypass(int chgnum)
{
	int rv;

	CPRINTS("bypass_chrg -> bypass");

	/* 1: Disable charge. */
	rv = rt9479_write16(chgnum, RT9479_REG_CHARGECURRENT, 0);
	if (rv)
		return rv;

	return EC_SUCCESS;
}

/*
 * Transition from NVDC to Bypass (automatic enter bypass)
 */
static enum ec_error_list rt9479_bypass_mode_auto_entry(int chgnum)
{
	const int charge_current = charge_manager_get_charger_current();
	const int charge_voltage = charge_manager_get_charger_voltage();
	int reg, vsys_target;
	int rv;
	timestamp_t deadline;

	CPRINTS("bypass mode enter");

	mutex_lock(&control3_mutex_rt9479);

	rv = rt9479_get_current(chgnum, &reg);
	if (rv || (!reg)) {
		CPRINTS("bypass mode enter check ichg fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	/* 1: Set AICR. */
	rt9479_set_input_current_limit(chgnum, charge_current);

	/* 2: Set Charge Voltage */
	vsys_target = min(charge_voltage - 256, CHARGE_V_MAX);
	rt9479_write16(chgnum, RT9479_REG_CHARGEVOLTAGE, vsys_target);

	/*
	 * 3: Set VBUSOK_TH < vsys_target
	 * to avoid exiting bypass mode due to heavy load.
	 */
	rt9479_write16(chgnum, RT9479_REG_VBUSOK_TH,
		       RT9479_MV_TO_VBUSOK_REG(vsys_target - 500));

	/* 4: Set MIVR < vsys_target */
	rt9479_write16(chgnum, RT9479_REG_MIVR,
		       RT9479_MV_TO_MIVR_REG(vsys_target - 500));

	/* 5: Set enter bypass mode(auto) = 1 */
	rv = rt9479_update16(chgnum, RT9479_REG_BYPASS_OPTION0, RT9479_BYP_AUTO,
			     MASK_SET);
	if (rv) {
		CPRINTS("bypass mode enter fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	/* 6: wait transition complete */
	deadline.val =
		get_time().val + RT9479_BYPASS_NVDC_TRANSITION_TIME_MS * MSEC;
	do {
		if (timestamp_expired(deadline, NULL)) {
			CPRINTS("bypass mode enter abort");
			rv = EC_ERROR_PARAM1;
			goto end;
		}
		crec_msleep(RT9479_BYPASS_NVDC_TRANSITION_TIME_MS / 10);
		rv = rt9479_read16(chgnum, RT9479_REG_INTERRUPT_STATUS, &reg);
		if (rv)
			continue;
		reg = !!(reg & RT9479_BYP_NVDC_READY_STAT);
	} while (!reg);

	/* 7: Check AC status */
	if (!rt9479_is_ac_present(chgnum)) {
		CPRINTS("bypass mode enter ac remove");
		rv = EC_ERROR_PARAM2;
		goto end;
	}

	/* 8: Check NVDC FET */
	rv = rt9479_read16(chgnum, RT9479_REG_CHARGERSTATUS1, &reg);
	reg = !!(reg & RT9479_NGATE_PG);
	if (rv || (reg)) {
		CPRINTS("bypass mode enter check ichg fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	/* 9: VBUS OK + NVDC FET Disable -> Bypass Transition Ready */
	CPRINTS("bypass mode enter success");
	rv = EC_SUCCESS;
end:
	mutex_unlock(&control3_mutex_rt9479);
	return rv;
}

/*
 * Transition from Bypass to NVDC (automatic exit bypass)
 */
static enum ec_error_list rt9479_bypass_mode_auto_exit(int chgnum)
{
	int rv, reg;
	timestamp_t deadline;

	CPRINTS("bypass mode exit");

	mutex_lock(&control3_mutex_rt9479);

	/* 1: check ichg = 0 */
	rv = rt9479_get_current(chgnum, &reg);
	if (rv || (!reg)) {
		CPRINTS("bypass mode exit check ichg fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	/* 2: Set exit bypass mode(auto) */
	rv = rt9479_update16(chgnum, RT9479_REG_BYPASS_OPTION0, RT9479_BYP_AUTO,
			     MASK_CLR);
	if (rv) {
		CPRINTS("bypass mode exit fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	/* 3: Check exit bypass done */
	deadline.val =
		get_time().val + RT9479_BYPASS_NVDC_TRANSITION_TIME_MS * MSEC;
	do {
		if (timestamp_expired(deadline, NULL)) {
			CPRINTS("bypass mode exit abort");
			rv = EC_ERROR_PARAM1;
			goto end;
		}
		crec_msleep(RT9479_BYPASS_NVDC_TRANSITION_TIME_MS / 10);
		rv = rt9479_read16(chgnum, RT9479_REG_INTERRUPT_STATUS, &reg);
		if (rv)
			continue;
		reg = !!(reg & RT9479_BYP_NVDC_READY_STAT);
	} while (!reg);

	/* 4: Check AC status */
	if (!rt9479_is_ac_present(chgnum))
		CPRINTS("bypass mode exit ac remove");

	/* 5: Check NVDC status */
	rv = rt9479_read16(chgnum, RT9479_REG_CHARGERSTATUS1, &reg);
	reg = !!(reg & RT9479_NGATE_PG);
	if (rv || (!reg)) {
		CPRINTS("bypass mode exit check ichg fail");
		rv = EC_ERROR_PARAM1;
		goto end;
	}

	CPRINTS("bypass mode exit success");
	rv = EC_SUCCESS;
end:
	mutex_unlock(&control3_mutex_rt9479);
	return rv;
}

static enum ec_error_list rt9479_enable_bypass_mode(int chgnum, bool enable)
{
	enum ec_error_list rv = EC_ERROR_UNKNOWN;

	if (enable) {
		if (rt9479_is_in_chrg(chgnum))
			rv = rt9479_nvdc_chrg_to_nvdc(chgnum);

		rv = rt9479_bypass_mode_auto_entry(chgnum);
	} else {
		if (rt9479_is_in_chrg(chgnum))
			rt9479_bypass_chrg_to_bypass(chgnum);

		rv = rt9479_bypass_mode_auto_exit(chgnum);
	}

	return rv;
}
#endif /* CONFIG_CHARGER_BYPASS_MODE */

static void rt9479_init(int chgnum)
{
	const struct battery_info *bi = battery_get_info();

	/*
	 * Charge Voltage
	 * Set the Charge Voltage to battery maximum
	 */
	if (rt9479_write16(chgnum, RT9479_REG_CHARGEVOLTAGE, bi->voltage_max))
		return;

	/*
	 * Minimum System Voltage
	 * Set the Minimum System Voltage to battery minimum,
	 */
	if (rt9479_write16(chgnum, RT9479_REG_VSYS_MIN, bi->voltage_min))
		return;

	/*
	 * Pre-Charge Current
	 * charge current will be clamped under 384mA(RSNS_RBAT=10mΩ
	 */

	/* Prochot debounce: INOM
	 * Set the INOM deglitch time to trigger PROCHOT: 1ms
	 */
	if (rt9479_field_update16(chgnum, RT9479_REG_PROCHOTOPTION0,
				  RT9479_INOM_PROC_DEG_MASK,
				  RT9479_INOM_PROC_DEG_1MS
					  << RT9479_INOM_PROC_DEG_SHIFT))
		return;

	/* Prochot debounce: ICRIT
	 * Set the ICRIT deglitch time to trigger PROCHOT: 800us
	 */
	if (rt9479_field_update16(chgnum, RT9479_REG_PROCHOTOPTION0,
				  RT9479_ICRIT_PROC_DEG_MASK,
				  RT9479_ICRIT_PROC_DEG_800US
					  << RT9479_ICRIT_PROC_DEG_SHIFT))
		return;

	/* Prochot debounce: IDCHG1
	 * Set the IDCHG1 deglitch time to trigger PROCHOT: 1.25s
	 */
	if (rt9479_field_update16(chgnum, RT9479_REG_PROCHOTOPTION1,
				  RT9479_IDCHG1_PROC_DEG_MASK,
				  RT9479_IDCHG1_PROC_DEG_1250MS
					  << RT9479_IDCHG1_PROC_DEG_SHIFT))
		return;

	/* Prochot debounce: IDCHG2
	 * Set the IDCHG2 deglitch time to trigger PROCHOT: 1.6ms
	 */
	if (rt9479_field_update16(chgnum, RT9479_REG_CHARGEOPTION4,
				  RT9479_IDCHG2_PROC_DEG_MASK,
				  RT9479_IDCHG2_PROC_DEG_1600US
					  << RT9479_IDCHG2_PROC_DEG_SHIFT))
		return;

	/* ACLIM Reload: Do not reload */
	if (rt9479_update16(chgnum, RT9479_REG_CONTROL3, RT9479_DIS_AICR_RELOAD,
			    MASK_SET))
		return;

	/*
	 * No need to proceed with the rest of init if we sysjump'd to this
	 * image as the input current limit has already been set.
	 */
	if (system_jumped_late())
		return;

	/*
	 * Average Input Current Regulation 1:
	 * Set the IAICR1 to the board's default
	 */
	if (rt9479_set_input_current_limit(
		    chgnum, CONFIG_CHARGER_DEFAULT_CURRENT_LIMIT))
		return;

	/*
	 * Average Input Current Regulation 2:
	 * Set the IAICR2 : 150% (Default)
	 */
	if (rt9479_set_input_current_limit_2(chgnum, RT9479_AICR2_150_PERCENT))
		return;

	CPRINTS("RT9479 init Successful!");
}

#ifdef CONFIG_CHARGE_RAMP_HW
static enum ec_error_list rt9479_set_hw_ramp(int chgnum, int enable)
{
	/* RT9479 MIVR always on */
	return 0;
}

static int rt9479_ramp_is_stable(int chgnum)
{
	/* RT9479 not support this function */
	return 0;
}

static int rt9479_ramp_is_detected(int chgnum)
{
	return 1;
}

static int rt9479_ramp_get_current_limit(int chgnum)
{
	int rv, val;

	rv = rt9479_enable_adc_oneshot(chgnum, ADC_IBUS);
	if (rv)
		return rv;

	rv = rt9479_get_adc_value(chgnum, ADC_IBUS, &val);
	if (rv)
		return rv;

	return val;
}
#endif /* CONFIG_CHARGE_RAMP_HW */

#ifdef CONFIG_CMD_CHARGER_DUMP
static void rt9479_dump_register(int chgnum)
{
	int reg;
	int regval;
	int rv;

	for (reg = 0x00; reg <= RT9479_REG_DEVICE_ID; reg++) {
		rv = rt9479_read16(chgnum, reg, &regval);
		ccprintf("[0x%4X] = 0x%4X\n", reg, regval);
	}
}
#endif /* CONFIG_CMD_CHARGER_DUMP */

const struct charger_drv rt9479_drv = {
	.init = &rt9479_init,
	.post_init = &rt9479_post_init,
	.get_info = &rt9479_get_info,
	.get_status = &rt9479_get_status,
	.set_mode = &rt9479_set_mode,
	.get_current = &rt9479_get_current,
	.set_current = &rt9479_set_current,
	.get_voltage = &rt9479_get_voltage,
	.set_voltage = &rt9479_set_voltage,
	.discharge_on_ac = &rt9479_discharge_on_ac,
	.get_vbus_voltage = &rt9479_get_vbus_voltage,
	.get_vsys_voltage = &rt9479_get_vsys_voltage,
	.set_input_current_limit = &rt9479_set_input_current_limit,
	.get_input_current_limit = &rt9479_get_input_current_limit,
	.manufacturer_id = &rt9479_manufacturer_id,
	.device_id = &rt9479_device_id,
	.set_frequency = &rt9479_set_frequency,
	.get_option = &rt9479_get_option,
	.set_option = &rt9479_set_option,
#ifdef CONFIG_CHARGE_RAMP_HW
	.set_hw_ramp = &rt9479_set_hw_ramp,
	.ramp_is_stable = &rt9479_ramp_is_stable,
	.ramp_is_detected = &rt9479_ramp_is_detected,
	.ramp_get_current_limit = &rt9479_ramp_get_current_limit,
#endif
#ifdef CONFIG_CHARGER_BYPASS_MODE
	.enable_bypass_mode = &rt9479_enable_bypass_mode,
#endif /* CONFIG_CHARGER_BYPASS_MODE */

#ifdef CONFIG_CMD_CHARGER_DUMP
	.dump_registers = &rt9479_dump_register,
#endif
#ifdef CONFIG_CHARGER_DUMP_PROCHOT
	.dump_prochot = &rt9479_dump_prochot_status,
#endif /* CONFIG_CHARGER_DUMP_PROCHOT */
};
