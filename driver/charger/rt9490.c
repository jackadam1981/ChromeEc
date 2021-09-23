/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Richtek 5A 1-4 cell buck-boost switching battery charger driver.
 */

#include "battery.h"
#include "battery_smart.h"
#include "charger.h"
#include "charge_manager.h"
#include "common.h"
#include "compile_time_macros.h"
#include "config.h"
#include "console.h"
#include "extpower.h"
#include "hooks.h"
#include "i2c.h"
#include "printf.h"
#include "rt9490.h"
#include "task.h"
#include "tcpm/tcpm.h"
#include "timer.h"
#include "usb_charge.h"
#include "usb_pd.h"
#include "util.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) \
	cprints(CC_CHARGER, "%s " format, "RT9490", ## args)

/* Charger parameters */
#define CHARGER_NAME    "rt9490"
#define CHARGE_V_MAX    18800
#define CHARGE_V_MIN    3000
#define CHARGE_V_STEP   10
#define CHARGE_I_MAX    5000
#define CHARGE_I_MIN    50
#define CHARGE_I_STEP   10
#define INPUT_I_MAX     3300
#define INPUT_I_MIN     100
#define INPUT_I_STEP    10

/* Charger parameters */
static const struct charger_info rt9490_charger_info = {
	.name         = CHARGER_NAME,
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

static const struct rt9490_init_setting default_init_setting = {
	.eoc_current = 200,
	.mivr = 4000,
	.boost_voltage = 5050,
	.boost_current = 1500,
};

static enum ec_error_list rt9490_read8(int chgnum, int reg, int *val)
{
	return i2c_read8(chg_chips[chgnum].i2c_port,
			chg_chips[chgnum].i2c_addr_flags, reg, val);
}

static enum ec_error_list rt9490_write8(int chgnum, int reg, int val)
{
	return i2c_write8(chg_chips[chgnum].i2c_port,
			chg_chips[chgnum].i2c_addr_flags, reg, val);
}

static enum ec_error_list rt9490_block_read(int chgnum, int reg,
					     uint8_t *val, int len)
{
	return i2c_read_block(chg_chips[chgnum].i2c_port,
			chg_chips[chgnum].i2c_addr_flags,
			reg, val, len);
}

static enum ec_error_list rt9490_block_write(int chgnum, int reg,
					     uint8_t *val, int len)
{
	return i2c_write_block(chg_chips[chgnum].i2c_port,
			chg_chips[chgnum].i2c_addr_flags,
			reg, val, len);
}

static int rt9490_update_bits(int chgnum, int reg, int mask, int val)
{
	int rv;
	int reg_val = 0;
	rv = rt9490_read8(chgnum, reg, &reg_val);
	if (rv)
		return rv;
	reg_val &= ~mask;
	reg_val |= (mask & val);
	rv = rt9490_write8(chgnum, reg, reg_val);
	return rv;
}

static inline int rt9490_set_bit(int chgnum, int reg, int mask)
{
	return rt9490_update_bits(chgnum, reg, mask, mask);
}

static inline int rt9490_clr_bit(int chgnum, int reg, int mask)
{
	return rt9490_update_bits(chgnum, reg, mask, 0x00);
}

static inline int rt9490_enable_hz(int chgnum, bool en)
{
	return (en ? rt9490_set_bit : rt9490_clr_bit)
		   (chgnum, RT9490_REG_CHG_CTRL0, RT9490_HZ_EN_MASK);
}

static const struct charger_info * rt9490_get_info(int chgnum)
{
	return &rt9490_charger_info;
}

static enum ec_error_list rt9490_get_current(int chgnum, int *current)
{
	int rv;
	uint16_t val = 0;
	const struct charger_info * const info = rt9490_get_info(chgnum);
	rv = rt9490_block_read(chgnum, RT9490_REG_ICHG_CTRL,
			(uint8_t*)&val, sizeof(val));
	if (rv)
		return rv;
	val = (val & RT9490_ICHG_MASK) >> RT9490_ICHG_SHIFT;
	val *= info->current_step;
	*current = CLAMP(val, info->current_min, info->current_max);
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_set_current(int chgnum, int current)
{
	uint16_t reg_ichg;
	const struct charger_info *const info = rt9490_get_info(chgnum);
	if (!IN_RANGE(current, info->current_min, info->current_max + 1))
		return EC_ERROR_PARAM2;
	reg_ichg = current / info->current_step;
	return rt9490_block_write(chgnum, RT9490_REG_ICHG_CTRL,
			(uint8_t*)&reg_ichg, sizeof(reg_ichg));
}

static enum ec_error_list rt9490_get_voltage(int chgnum, int *voltage)
{
	int rv;
	uint16_t val = 0;
	const struct charger_info * const info = rt9490_get_info(chgnum);
	rv = rt9490_block_read(chgnum, RT9490_REG_VCHG_CTRL,
			(uint8_t*)&val, sizeof(val));
	if (rv)
		return rv;
	val = val & RT9490_CV_MASK;
	val *= info->voltage_step;
	*voltage = CLAMP(val, info->voltage_min, info->voltage_max);
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_set_voltage(int chgnum, int voltage)
{
	uint16_t reg_cv;
	const struct charger_info *const info = rt9490_get_info(chgnum);
	if (!IN_RANGE(voltage, info->current_min, info->current_max + 1))
		return EC_ERROR_PARAM2;
	reg_cv = voltage / info->voltage_step;
	return rt9490_block_write(chgnum, RT9490_REG_VCHG_CTRL,
			(uint8_t*)&reg_cv, sizeof(reg_cv));
}

#ifdef CONFIG_CHARGER_OTG
static enum ec_error_list rt9490_enable_otg_power(int chgnum, int enabled)
{
	return (enabled ? rt9490_set_bit : rt9490_clr_bit)
		(chgnum, RT9490_REG_CHG_CTRL3, RT9490_OTG_EN_MASK);
}

enum ec_error_list rt9490_set_otg_current_voltage(int chgnum, int output_current, int output_voltage)
{
	int rv;
	uint16_t reg_cur, reg_vol;
	CPRINTS("set otg cur:%d, volt:%d", output_current, output_voltage);
	if (!IN_RANGE(output_current, RT9490_IOTG_MIN, RT9490_IOTG_MAX + 1))
		return EC_ERROR_PARAM2;
	if (!IN_RANGE(output_voltage, RT9490_VOTG_MIN, RT9490_VOTG_MAX + 1))
		return EC_ERROR_PARAM3;
	reg_cur = (output_current - RT9490_VOTG_MIN) / RT9490_IOTG_STEP;
	reg_vol = (output_voltage - RT9490_VOTG_MIN) / RT9490_VOTG_STEP;
	rv = rt9490_block_write(chgnum, RT9490_REG_IOTG_REGU,
			(uint8_t*)&reg_cur, sizeof(reg_cur));
	if (rv)
		return rv;
	return rt9490_block_write(chgnum, RT9490_REG_VOTG_REGU,
			(uint8_t*)&reg_vol, sizeof(reg_vol));
}

static int rt9490_is_sourcing_otg_power(int chgnum, int port)
{
	int val;
	if (rt9490_read8(chgnum, RT9490_REG_CHG_CTRL3, &val))
		return 0;
	return !!(val & RT9490_OTG_EN_MASK);
}
#endif

/* Reset all registers' value to default */
static int rt9490_reset_chip(int chgnum)
{
	int rv = 0;

	CPRINTS("%s\r\n", __func__);

	/* disable hz before reset chip */
	rv = rt9490_enable_hz(chgnum, false);
	if (rv < 0) {
		CPRINTS("%s: disable hz fail\r\n", __func__);
		return rv;
	}

	return rt9490_set_bit(chgnum, RT9490_REG_EOC_CTRL, RT9490_RST_ALL_MASK);
}

static inline int __rt9490_enable_chgdet_flow(int chgnum, bool en)
{
	return (en ? rt9490_set_bit : rt9490_clr_bit)
		(chgnum, RT9490_REG_CHG_CTRL2, RT9490_BC12_EN_MASK);
}

static inline int __rt9490_enable_te(int chgnum, bool en)
{
	//CPRINTS("%s: en = %d\r\n", __func__, en);
	return (en ? rt9490_set_bit : rt9490_clr_bit)
		(chgnum, RT9490_REG_CHG_CTRL0, RT9490_TE_EN_MASK);
}

static inline int __rt9490_enable_safety_timer(int chgnum, bool en)
{
	//CPRINTS("%s: en = %d\r\n", __func__, en);
	return (en ? rt9490_set_bit : rt9490_clr_bit)
			(chgnum, RT9490_REG_SAFETY_TMR_CTRL, RT9490_EN_FASTCHG_TMR_MASK);
}

static inline int rt9490_enable_wdt(int chgnum, bool en)
{
	//CPRINTS("%s: en = %d\r\n", __func__, en);
	if (en) {
		return rt9490_update_bits(chgnum, RT9490_REG_CHG_CTRL1,
				RT9490_WDT_DEFAULT_VAL, RT9490_WDT_MASK);
	} else {
		return rt9490_clr_bit(chgnum, RT9490_REG_CHG_CTRL1, RT9490_WDT_MASK);
	}
}

static inline int rt9490_set_mivr(int chgnum, unsigned int mivr)
{
	uint8_t reg_mivr = mivr / RT9490_MIVR_STEP;
	CPRINTS("mivr=%d", mivr);
	return rt9490_write8(chgnum, RT9490_REG_MIVR_CTRL, reg_mivr);
}

static inline int rt9490_set_ieoc(int chgnum, unsigned int ieoc)
{
	uint8_t reg_ieoc = ieoc / RT9490_IEOC_STEP;
	CPRINTS("ieoc=%d", ieoc);
	return rt9490_update_bits(chgnum, RT9490_REG_EOC_CTRL, RT9490_IEOC_MASK,
			reg_ieoc);
}

static inline int rt9490_enable_jeita(int chgnum, bool en)
{
	CPRINTS("%s: en = %d\r\n", __func__, en);
	return (en ? rt9490_clr_bit : rt9490_set_bit)
			(chgnum, RT9490_REG_JEITA_CTRL1, RT9490_DIS_JEITA_FN_MASK);
}

static inline int rt9490_enable_adc(int chgnum, bool en)
{
	return (en ? rt9490_set_bit : rt9490_clr_bit)
			(chgnum, RT9490_REG_ADC_CTRL, RT9490_ADC_EN_MASK);
}

static int rt9490_set_iprec(int chgnum, unsigned int iprec)
{
	uint8_t reg_iprec = iprec / RT9490_IPRE_CHG_STEP;
	CPRINTS("iprec=%d", iprec);
	return rt9490_update_bits(chgnum, RT9490_REG_PRE_CHG,
			RT9490_IPRE_CHG_MASK,
			reg_iprec << RT9490_IPREC_SHIFT);
}

static int rt9490_init_setting(int chgnum)
{
	int rv = 0;
	const struct battery_info *batt_info = battery_get_info();
#ifdef CONFIG_CHARGER_OTG
	/*  Disable boost-mode output voltage */
	rv = rt9490_enable_otg_power(chgnum, 0);
	if (rv)
		return rv;
	rv = rt9490_set_otg_current_voltage(chgnum,
			default_init_setting.boost_current,
			default_init_setting.boost_voltage);
	if (rv)
		return rv;
#endif
	/* Disable BC 1.2 detection by default. It will be enabled on demand */
	rv = __rt9490_enable_chgdet_flow(chgnum, false);
	if (rv)
		return rv;
	/* Disable WDT */
	rv = rt9490_enable_wdt(chgnum, 0);
	if (rv)
		return rv;
	/* Disable battery thermal protection */
	rv = rt9490_set_bit(chgnum, RT9490_REG_ADD_CTRL0, RT9490_JEITA_COLD_HOT);
	if (rv)
		return rv;
	/* Disable charge timer */
	rv = rt9490_clr_bit(chgnum, RT9490_REG_SAFETY_TMR_CTRL, RT9490_EN_TMR_MASK);
	if (rv)
		return rv;
	rv = rt9490_set_mivr(chgnum, default_init_setting.mivr);
	if (rv)
		return rv;
	rv = rt9490_set_ieoc(chgnum, default_init_setting.eoc_current);
	if (rv)
		return rv;
	rv = rt9490_set_iprec(chgnum, batt_info->precharge_current);
	if (rv)
		return rv;
	rv = rt9490_enable_adc(chgnum, true);
	if (rv)
		return rv;
	 /* Workaround for VSYS ADC issue */
	rv = rt9490_set_bit(chgnum, RT9490_REG_ADC_CHANNEL0, RT9490_VSYS_ADC_DIS_MASK);

#if 1 /* work around for IBUS ADC unstable issue */
	rv = rt9490_set_bit(chgnum, RT9490_REG_ADC_CHANNEL0, RT9490_VSYS_ADC_DIS_MASK);
    if (rv)
    	return rv;

    rv = rt9490_write8(chgnum, 0xF1, 0x69);
    rv |= rt9490_write8(chgnum, 0xF2, 0x96);
    rv |= rt9490_write8(chgnum, 0x52, 0xC4);
    if (rv)
    	return rv;

    rv = rt9490_clr_bit(chgnum, RT9490_REG_ADC_CHANNEL0, RT9490_VSYS_ADC_DIS_MASK);
    if (rv)
    	return rv;
#endif
	return EC_SUCCESS;
}

static void rt9490_init(int chgnum)
{
	int ret = rt9490_init_setting(chgnum);
	CPRINTS("init%d %s(%d)", chgnum, ret ? "fail" : "good", ret);
}

static enum ec_error_list rt9490_post_init(int chgnum)
{
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_get_status(int chgnum, int *status)
{
	int rv, val = 0;
	rv = rt9490_read8(chgnum, RT9490_REG_CHG_CTRL0, &val);
	if (rv)
		return rv;
	val = (val & RT9490_CHG_EN_MASK) >> RT9490_CHG_EN_SHIFT;
	if (!val)
		*status |= CHARGER_CHARGE_INHIBITED;

	rv = rt9490_read8(chgnum, RT9490_REG_FAULT_STATUS0, &val);
	if (rv)
		return rv;
	if (val & RT9490_VBAT_OVP_STAT_MASK)
		*status |= CHARGER_VOLTAGE_OR;

	rv = rt9490_read8(chgnum, RT9490_REG_CHG_STATUS4, &val);
	if (rv)
		return rv;
	if (val & RT9490_JEITA_COLD_MASK) {
		*status |= CHARGER_RES_COLD;
		*status |= CHARGER_RES_UR;
	}
	if (val & RT9490_JEITA_COOL_MASK) {
		*status |= CHARGER_RES_COLD;
	}
	if (val & RT9490_JEITA_WARM_MASK) {
		*status |= CHARGER_RES_HOT;
	}
	if (val & RT9490_JEITA_HOT_MASK) {
		*status |= CHARGER_RES_HOT;
		*status |= CHARGER_RES_OR;
	}
	return EC_SUCCESS;
}

static int rt9490_reset_to_zero(int chgnum)
{
	int rv;
	rv = rt9490_set_current(chgnum, 0);
	if (rv)
		return rv;
	rv = rt9490_set_voltage(chgnum, 0);
	if (rv)
		return rv;
	return rt9490_enable_hz(chgnum, 1);
}

static enum ec_error_list rt9490_set_mode(int chgnum, int mode)
{
	int rv;
	if (mode & CHARGE_FLAG_POR_RESET) {
		rv = rt9490_reset_chip(chgnum);
		if (rv)
			return rv;
	}
	if (mode & CHARGE_FLAG_RESET_TO_ZERO) {
		rv = rt9490_reset_to_zero(chgnum);
		if (rv)
			return rv;
	}
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_get_actual_current(int chgnum, int *current)
{
	int rv;
	uint8_t reg_val[2];
	rv = rt9490_block_read(chgnum, RT9490_REG_IBAT_ADC,
			reg_val, sizeof(reg_val));
	if (rv)
		return rv;
	*current = (int)((reg_val[0] << 8) + reg_val[1]) * 1000;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_get_actual_voltage(int chgnum, int *voltage)
{
	int rv;
	uint8_t reg_val[2];
	rv = rt9490_block_read(chgnum, RT9490_REG_VBAT_ADC,
			reg_val, sizeof(reg_val));
	if (rv)
		return rv;
	*voltage = (int)((reg_val[0] << 8) + reg_val[1]) * 1000;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_discharge_on_ac(int chgnum, int enable)
{
	return rt9490_enable_hz(chgnum, enable);
}

static enum ec_error_list rt9490_get_vbus_voltage(int chgnum, int port, int *voltage)
{
	int rv;
	uint8_t reg_val[2];
	rv = rt9490_block_read(chgnum, RT9490_REG_VBUS_ADC,
			reg_val, sizeof(reg_val));
	if (rv)
		return rv;
	*voltage = (int)((reg_val[0] << 8) + reg_val[1]) * 1000;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_set_input_current_limit(int chgnum, int input_current)
{
	uint16_t reg_val = input_current / RT9490_AICR_STEP;
	return rt9490_block_write(chgnum, RT9490_REG_AICR_CTRL,
			(uint8_t*)&reg_val, sizeof(reg_val));
}

static enum ec_error_list rt9490_get_input_current_limit(int chgnum, int *input_current)
{
	int rv;
	uint16_t val = 0;
	rv = rt9490_block_read(chgnum, RT9490_REG_AICR_CTRL,
			(uint8_t*)&val, sizeof(val));
	if (rv)
		return rv;
	val = (val & RT9490_AICR_MASK) >> RT9490_AICR_SHIFT;
	*input_current = val * RT9490_AICR_STEP;
	if (*input_current < RT9490_AICR_MIN)
		*input_current = RT9490_AICR_MIN;
	else if (*input_current > RT9490_AICR_MAX)
		*input_current = RT9490_AICR_MAX;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_get_input_current(int chgnum, int *input_current)
{
	int rv;
	uint8_t reg_val[2];
	rv = rt9490_block_read(chgnum, RT9490_REG_IBUS_ADC,
			reg_val, sizeof(reg_val));
	if (rv)
		return rv;
	*input_current = (int)((reg_val[0] << 8) + reg_val[1]) * 1000;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_manufacturer_id(int chgnum, int *id)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static enum ec_error_list rt9490_device_id(int chgnum, int *id)
{
	int rv;
	rv = rt9490_read8(chgnum, RT9490_REG_DEVICE_INFO, id);
	if (rv == EC_SUCCESS)
		*id &= RT9490_DEVICE_INFO_MASK;
	return rv;
}

static enum ec_error_list rt9490_get_option(int chgnum, int *option)
{
	/* Ignored: does not exist */
	*option = 0;
	return EC_SUCCESS;
}

static enum ec_error_list rt9490_set_option(int chgnum, int option)
{
	/* Ignored: does not exist */
	return EC_SUCCESS;
}
#ifdef CONFIG_CHARGE_RAMP_HW
static enum ec_error_list rt9490_set_hw_ramp(int chgnum, int enable)
{
	int rv;
	if (enable) {
		rv = rt9490_set_bit(chgnum, RT9490_REG_CHG_CTRL0, RT9490_AICC_EN_MASK);
		if (rv)
			return rv;
		rv = rt9490_set_bit(chgnum, RT9490_REG_CHG_CTRL0, RT9490_FORCE_AICC_MASK);
	} else {
		rv = rt9490_clr_bit(chgnum, RT9490_REG_CHG_CTRL0, RT9490_AICC_EN_MASK);
	}
	return rv;
}

static int rt9490_ramp_is_stable(int chgnum)
{
	int rv;
	int val = 0;
	rv = rt9490_read8(chgnum, RT9490_REG_CHG_CTRL0, &val);
	val = (val & RT9490_FORCE_AICC_MASK) >> RT9490_FORCE_AICC_SHIFT;
	return (!rv && !val);
}

static int rt9490_ramp_is_detected(int chgnum)
{
	return true;
}

static int rt9490_ramp_get_current_limit(int chgnum)
{
	int rv;
	int input_current = 0;
	rv = rt9490_get_input_current_limit(chgnum, &input_current);
	return rv ? -1 : input_current;
}
#endif
static enum ec_error_list rt9490_set_vsys_compensation(int chgnum,
							struct ocpc_data *o,
							int current_ma,
							int voltage_mv)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static enum ec_error_list rt9490_is_icl_reached(int chgnum, bool *reached)
{
	return EC_ERROR_UNIMPLEMENTED;
}

static enum ec_error_list rt9490_enable_linear_charge(int chgnum, bool enable)
{
	return EC_ERROR_UNIMPLEMENTED;
}

const struct charger_drv rt9490_drv = {
	.init = &rt9490_init,
	.post_init = &rt9490_post_init,
	.get_info = &rt9490_get_info,
	.get_status = &rt9490_get_status,
	.set_mode = &rt9490_set_mode,
#ifdef CONFIG_CHARGER_OTG
	.enable_otg_power = &rt9490_enable_otg_power,
	.set_otg_current_voltage = &rt9490_set_otg_current_voltage,
	.is_sourcing_otg_power = &rt9490_is_sourcing_otg_power,
#endif
	.get_current = &rt9490_get_current,
	.set_current = &rt9490_set_current,
	.get_voltage = &rt9490_get_voltage,
	.set_voltage = &rt9490_set_voltage,
	.get_actual_current = &rt9490_get_actual_current,
	.get_actual_voltage = &rt9490_get_actual_voltage,
	.discharge_on_ac = &rt9490_discharge_on_ac,
	.get_vbus_voltage = &rt9490_get_vbus_voltage,
	.set_input_current_limit = &rt9490_set_input_current_limit,
	.get_input_current_limit = &rt9490_get_input_current_limit,
	.get_input_current = &rt9490_get_input_current,
	.manufacturer_id = &rt9490_manufacturer_id,
	.device_id = &rt9490_device_id,
	.get_option = &rt9490_get_option,
	.set_option = &rt9490_set_option,
#ifdef CONFIG_CHARGE_RAMP_HW
	.set_hw_ramp = &rt9490_set_hw_ramp,
	.ramp_is_stable = &rt9490_ramp_is_stable,
	.ramp_is_detected = &rt9490_ramp_is_detected,
	.ramp_get_current_limit = &rt9490_ramp_get_current_limit,
#endif
	.set_vsys_compensation = &rt9490_set_vsys_compensation,
	.is_icl_reached = &rt9490_is_icl_reached,
	.enable_linear_charge = &rt9490_enable_linear_charge,
};

