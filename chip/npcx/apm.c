/* Copyright (c) 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NPCX-specific APM module for Chrome EC */

#include "apm_chip.h"
#include "common.h"
#include "registers.h"
#include "util.h"
#include "wov_chip.h"

struct apm_config apm_conf;

static uint32_t adc_frequency[] = {
	8000,
	0xFFFF,
	12000,
	16000,
	0xFFFF,
	24000,
	32000,
	0xFFFF,
	48000,
	0xFFFF,
	0xFFFF,
	0xFFFF,
	0xFFFF
};

static uint32_t apm_indirect_reg[][3] = {
	{(NPCX_APM_BASE_ADDR + 0x034), (NPCX_APM_BASE_ADDR + 0x038)},
	{(NPCX_APM_BASE_ADDR + 0x04C), (NPCX_APM_BASE_ADDR + 0x050)},
	{(NPCX_APM_BASE_ADDR + 0x05C), (NPCX_APM_BASE_ADDR + 0x060)}
};

#define APM_CNTRL_REG 0
#define APM_DATA_REG 1

/**
 * Reads data indirect register.
 *
 * @param   reg_offset    - Indirect register APM_MIX_REG, APM_ADC_AGC_REG or
 *                          APM_VAD_REG.
 *          indirect_addr - Indirect access address.
 * @return  The read data.
 */
static uint8_t apm_read_indirect_data(enum apm_indirect_reg_offset reg_offset,
				      uint8_t indirect_addr)
{
	/* Set the indirect access address. */
	SET_FIELD(REG8(apm_indirect_reg[reg_offset][APM_CNTRL_REG]),
			NPCX_APM_CONTROL_ADD, indirect_addr);

	/* Read command. */
	CLEAR_BIT(REG8(apm_indirect_reg[reg_offset][APM_CNTRL_REG]),
			NPCX_APM_CONTROL_LOAD);

	/* Get the data. */
	return REG8(apm_indirect_reg[reg_offset][APM_DATA_REG]);
}

/**
 * Writes data indirect register.
 *
 * @param   reg_offset    - Indirect register APM_MIX_REG, APM_ADC_AGC_REG or
 *                          APM_VAD_REG.
 *          indirect_addr - Indirect access address.
 *          value         - Written value.
 * @return  None
 */
static void apm_write_indirect_data(enum apm_indirect_reg_offset reg_offset,
				uint8_t indirect_addr, uint8_t value)
{
	/* Set the indirect access address. */
	SET_FIELD(REG8(apm_indirect_reg[reg_offset][APM_CNTRL_REG]),
			NPCX_APM_CONTROL_ADD, indirect_addr);

	/* Set the data. */
	REG8(apm_indirect_reg[reg_offset][APM_DATA_REG]) = value;

	/* Write command. */
	SET_BIT(REG8(apm_indirect_reg[reg_offset][APM_CNTRL_REG]),
			NPCX_APM_CONTROL_LOAD);
}

/**
 * Sets the ADC DMIC rate.
 *
 * @param   rate      - ADC digital microphone rate
 * @return  None
 */
void apm_set_adc_dmic_config_l(enum apm_dmic_rate rate)
{
	SET_FIELD(NPCX_APM_CR_DMIC, NPCX_APM_CR_DMIC_ADC_DMIC_RATE, rate);
}

/**
 * Sets VAD DMIC rate.
 *
 * @param   rate    - VAD DMIC rate
 *
 * @return  None
 */
void apm_set_vad_dmic_rate_l(enum apm_dmic_rate rate)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG);

	/* Set VAD_0 register. */
	SET_FIELD(vad_data, NPCX_VAD_0_VAD_DMIC_FREQ, rate);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG, vad_data);
}

/*****************************************************************************/
/* IC specific low-level driver */

/**
 * Gets APM interrupt mode.
 *
 * @param   None
 * @return  APM interrupt mode.
 */
enum apm_interrupt_mode apm_get_intr_mode(void)
{
	return ((enum apm_interrupt_mode)GET_FIELD(NPCX_APM_ICR,
				NPCX_APM_ICR_INTR_MODE));
}

/**
 * Gets the ADC frequency.
 *
 * @param   None
 * @return  ADC frequency code
 */
enum apm_adc_frequency apm_adc_get_freq(void)
{
	return ((enum apm_adc_frequency)GET_FIELD(NPCX_APM_FCR_ADC,
				NPCX_APM_FCR_ADC_ADC_FREQ));
}

/**
 * Translates from ADC frequency code to a real value
 *
 * @param   adc_freq    - ADC frequency.
 * @return  ADC frequency value, 0xFFFF in case of wrong value.
 */
uint32_t apm_adc_freq_code_2_val(enum apm_adc_frequency adc_freq)
{
	if (adc_freq > APM_ADC_FREQ_48_000_KHZ)
		return 0xFFFF;

	return adc_frequency[adc_freq];
}

/**
 * Translates from ADC real value to frequency code
 *
 * @param   adc_freq_val   - ADC frequency.
 * @return  ADC frequency code, 0xFFFF in case of wrong value.
 */
enum apm_adc_frequency apm_adc_freq_val_2_code(uint32_t adc_freq_val)
{
	enum apm_adc_frequency freq_code;

	switch (adc_freq_val) {
	case 8000:
		freq_code = APM_ADC_FREQ_8_000_KHZ;
		break;
	case 12000:
		freq_code = APM_ADC_FREQ_12_000_KHZ;
		break;
	case 16000:
		freq_code = APM_ADC_FREQ_16_000_KHZ;
		break;
	case 24000:
		freq_code = APM_ADC_FREQ_24_000_KHZ;
		break;
	case 32000:
		freq_code = APM_ADC_FREQ_32_000_KHZ;
		break;
	case 48000:
		freq_code = APM_ADC_FREQ_48_000_KHZ;
		break;
	default:
		freq_code = APM_ADC_FREQ_WRONG;
		break;
	}

	return freq_code;
}

/**
 * Enables/Disables VAD functionality.
 *
 * @param   zero_cross_detec_enable - Enable Zero Crossing Detection
 *                                    functionality
 *          vad_adc_wakeup_enable   - Enables automatic ADC filter chain wakeup
 *                                    when voice is detected.
 *          vad_clk_freq            - Digital microphone interface clock
 *                                    frequency selection
 *          chan_src                - Processed digital microphone channel
 *                                    selection.
 *          power_level_sens        - Power level sensitivity.
 * @return  None
 */
void apm_vad_config(int zero_cross_detec_enable, int vad_adc_wakeup_enable,
		enum apm_dmic_rate vad_clk_freq,
		enum apm_vad_in_channel_src chan_src, uint8_t power_level_sens)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG);

/* Set VAD_0 register. */

#if 0
	/* This bit must be kept at the default value. */
	if (zero_cross_detec_enable == true)
		SET_BIT(vad_data, NPCX_VAD_0_ZCD_EN);
	else
		CLEAR_BIT(vad_data, NPCX_VAD_0_ZCD_EN);
#endif
	if (vad_adc_wakeup_enable)
		SET_BIT(vad_data, NPCX_VAD_0_VAD_ADC_WAKEUP);
	else
		CLEAR_BIT(vad_data, NPCX_VAD_0_VAD_ADC_WAKEUP);

	SET_FIELD(vad_data, NPCX_VAD_0_VAD_DMIC_FREQ, vad_clk_freq);
	SET_FIELD(vad_data, NPCX_VAD_0_VAD_INSEL, chan_src);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG, vad_data);

	/* Set VAD_1 register. */

	vad_data = 0;

	SET_FIELD(vad_data, NPCX_VAD_1_VAD_POWER_SENS, power_level_sens);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_1_REG, vad_data);
}

/**
 * Enables/Disables APM  module.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  None
 */
void apm_enable(int enable)
{
	if (enable)
		CLEAR_BIT(NPCX_APM_CR_APM, NPCX_APM_CR_APM_PD);
	else
		SET_BIT(NPCX_APM_CR_APM, NPCX_APM_CR_APM_PD);
}

/**
 * Enables/Disables voice activity detected interrupt.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  APM interrupt mode.
 */
void apm_enable_vad_interrupt(int enable)
{
	wov_interrupt_enable(WOV_VAD_INT_INDX, enable);
	wov_interrupt_enable(WOV_VAD_WAKE_INDX, enable);
	if (enable)
		CLEAR_BIT(NPCX_APM_IMR, NPCX_APM_IMR_VAD_DTC_MASK);
	else
		SET_BIT(NPCX_APM_IMR, NPCX_APM_IMR_VAD_DTC_MASK);
}

/**
 * Enables/Disables ADC.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  None
 */
void apm_adc_enable(int enable)
{
	if (enable) {
		CLEAR_BIT(NPCX_APM_AICR_ADC, NPCX_APM_AICR_ADC_PD_AICR_ADC);
		SET_FIELD(NPCX_APM_AICR_ADC,
				NPCX_APM_AICR_ADC_ADC_AUDIOIF, 0x00);
	} else {
		SET_BIT(NPCX_APM_AICR_ADC, NPCX_APM_AICR_ADC_PD_AICR_ADC);
		SET_FIELD(NPCX_APM_AICR_ADC,
				NPCX_APM_AICR_ADC_ADC_AUDIOIF, 0x03);
	}
}

/**
 * sets the ADC frequency.
 *
 * @param   adc_freq    - ADC frequency.
 * @return  None
 */
void apm_adc_set_freq(enum apm_adc_frequency adc_freq)
{
	SET_FIELD(NPCX_APM_FCR_ADC, NPCX_APM_FCR_ADC_ADC_FREQ, adc_freq);
}

/**
 * Configures the ADC.
 *
 * @param   hpf_enable  - High pass filter enabled flag, 1 means enable
 *          filter_mode - ADC wind noise filter mode.
 *          adc_freq    - ADC frequency.
 * @return  None
 */
void apm_adc_config(int hpf_enable,
		enum apm_adc_wind_noise_filter_mode filter_mode,
		enum apm_adc_frequency adc_freq)
{
	if (hpf_enable)
		SET_BIT(NPCX_APM_FCR_ADC, NPCX_APM_FCR_ADC_ADC_HPF);
	else
		CLEAR_BIT(NPCX_APM_FCR_ADC, NPCX_APM_FCR_ADC_ADC_HPF);

	SET_FIELD(NPCX_APM_FCR_ADC, NPCX_APM_FCR_ADC_ADC_WNF, filter_mode);

	SET_FIELD(NPCX_APM_FCR_ADC, NPCX_APM_FCR_ADC_ADC_FREQ, adc_freq);
}

/**
 * Enables/Disables Digital Microphone.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  None
 */
void apm_dmic_enable(int enable)
{
	if (enable)
		CLEAR_BIT(NPCX_APM_CR_DMIC, NPCX_APM_CR_DMIC_PD_DMIC);
	else
		SET_BIT(NPCX_APM_CR_DMIC, NPCX_APM_CR_DMIC_PD_DMIC);
}

/**
 * Sets the ADC DMIC rate.
 *
 * @param   rate      - ADC digital microphone rate
 * @return  None
 */
void apm_set_adc_dmic_config(enum apm_dmic_rate rate)
{
	apm_conf.adc_dmic_rate = rate;
}

/**
 * Configures Digital Mixer
 *
 * @param   mix_left  - Mixer left channel output selection on ADC path.
 *          mix_right - Mixer right channel output selection on ADC path.
 * @return  None
 */
void apm_digital_mixer_config(enum apm_dig_mix mix_left,
			      enum apm_dig_mix mix_right)
{
	uint8_t mix_2 = 0;

	SET_FIELD(mix_2, NPCX_APM_MIX_2_AIADCL_SEL, mix_left);
	SET_FIELD(mix_2, NPCX_APM_MIX_2_AIADCR_SEL, mix_right);

	apm_write_indirect_data(APM_MIX_REG, APM_INDIRECT_MIX_2_REG, mix_2);
}

/**
 * Enables/Disables the VAD functionality.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  None
 */
void apm_vad_enable(int enable)
{
	if (enable)
		SET_BIT(NPCX_APM_CR_VAD, NPCX_APM_CR_VAD_VAD_EN);
	else
		CLEAR_BIT(NPCX_APM_CR_VAD, NPCX_APM_CR_VAD_VAD_EN);
}

/**
 * Enables/Disables VAD ADC wakeup
 *
 * @param   enable - 1 enable, 0 disable.
 *
 * @return  None
 */
void apm_vad_adc_wakeup_enable(int enable)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG);

	if (enable)
		SET_BIT(vad_data, NPCX_VAD_0_VAD_ADC_WAKEUP);
	else
		CLEAR_BIT(vad_data, NPCX_VAD_0_VAD_ADC_WAKEUP);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG, vad_data);
}

/**
 * Sets VAD DMIC rate.
 *
 * @param   rate    - VAD DMIC rate
 *
 * @return  None
 */
void apm_set_vad_dmic_rate(enum apm_dmic_rate rate)
{
	apm_conf.vad_dmic_rate = rate;
}

/**
 * Sets VAD Input chanel.
 *
 * @param   chan_src                - Processed digital microphone channel
 *                                    selection.
 * @return  None
 */
void apm_set_vad_input_channel(enum apm_vad_in_channel_src chan_src)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG);

	SET_FIELD(vad_data, NPCX_VAD_0_VAD_INSEL, chan_src);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_0_REG, vad_data);
}

/**
 * Sets VAD sensitivity.
 *
 * @param   sensitivity_db - VAD sensitivity in db.
 * @return  None
 */
void apm_set_vad_sensitivity(uint8_t sensitivity_db)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_1_REG);

	SET_FIELD(vad_data, NPCX_VAD_1_VAD_POWER_SENS, sensitivity_db);

	apm_write_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_1_REG, vad_data);
}

/**
 * Gets VAD sensitivity.
 *
 * @param   None.
 * @return  VAD sensitivity in db
 */
uint8_t apm_get_vad_sensitivity(void)
{
	uint8_t vad_data;

	vad_data = apm_read_indirect_data(APM_VAD_REG, APM_INDIRECT_VAD_1_REG);

	return GET_FIELD(vad_data, NPCX_VAD_1_VAD_POWER_SENS);
}

/**
 * Restarts VAD functionality.
 *
 * @param   None
 * @return  None
 */
void apm_vad_restart(void)
{
	SET_BIT(NPCX_APM_CR_VAD_CMD, NPCX_APM_CR_VAD_CMD_VAD_RESTART);
}

/**
 * Restarts VAD functionality.
 *
 * @param   gain_coupling   - ADC digital gain coupling (independent or
 *                            rigth tracks left).
 *          left_chan_gain  - Left channel ADC digital gain programming value.
 *          right_chan_gain - Right channel ADC digital gain programming value.
 * @return  EC_ERROR_INVAL or EC_SUCCESS
 */
enum ec_error_list apm_adc_gain_config(enum apm_adc_gain_coupling gain_coupling,
		uint8_t left_chan_gain, uint8_t right_chan_gain)
{
	/* Check parameters validity. */
	if ((left_chan_gain > 0x2B) || (right_chan_gain > 0x2B))
		return EC_ERROR_INVAL;

	/* Set gain coupling.*/
	if (gain_coupling == APM_ADC_CHAN_GAINS_INDEPENDENT)
		CLEAR_BIT(NPCX_APM_GCR_ADCL, NPCX_APM_GCR_ADCL_LRGID);
	else
		SET_BIT(NPCX_APM_GCR_ADCL, NPCX_APM_GCR_ADCL_LRGID);

	/* set channels gains. */
	SET_FIELD(NPCX_APM_GCR_ADCL, NPCX_APM_GCR_ADCL_GIDL, left_chan_gain);
	SET_FIELD(NPCX_APM_GCR_ADCR, NPCX_APM_GCR_ADCR_GIDR, right_chan_gain);

	return EC_SUCCESS;
}

/**
 * Enables/Disables the automatic gain.
 *
 * @param   enable - enabled flag, 1 means enable
 * @return  None
 */
void apm_auto_gain_cntrl_enable(int enable)
{
	if (enable)
		SET_BIT(NPCX_APM_CR_ADC_AGC, NPCX_APM_CR_ADC_AGC_ADC_AGC_EN);
	else
		CLEAR_BIT(NPCX_APM_CR_ADC_AGC, NPCX_APM_CR_ADC_AGC_ADC_AGC_EN);
}

/**
 * Enables/Disables the automatic gain.
 *
 * @param stereo_enable   - Stereo enabled flag, 1 means enable.
 *        agc_target      - Target output level of the ADC.
 *        nois_gate_en    - Noise Gate system select. 1 means enable.
 *        nois_gate_thold - Noise Gate Threshold Value.
 *        hold_time       - Hold time before starting AGC adjustment to the
 *                          TARGET value.
 *        attack_time     - Attack time - gain ramp down.
 *        decay_time      - Decay time - gain ramp up.
 *        gain_max        - Maximum Gain Value to apply to the ADC path.
 *        gain_min        - Minimum Gain Value to apply to the ADC path.
 * @return  EC_ERROR_INVAL or EC_SUCCESS
 */
enum ec_error_list apm_adc_auto_gain_config(int stereo_enable,
	enum apm_adc_target_out_level agc_target, int nois_gate_en,
	enum apm_noise_gate_threshold nois_gate_thold,
	enum apm_agc_adj_hold_time hold_time,
	enum apm_gain_ramp_time attack_time, enum apm_gain_ramp_time decay_time,
	enum apm_gain_values gain_max, enum apm_gain_values gain_min)
{
	uint8_t gain_data = 0;

	/* Check parameters validity. */

	if (gain_min > gain_max)
		return EC_ERROR_INVAL;

	/* Set the parameters. */

	if (stereo_enable)
		CLEAR_BIT(gain_data, NPCX_ADC_AGC_0_AGC_STEREO);
	else
		SET_BIT(gain_data, NPCX_ADC_AGC_0_AGC_STEREO);

	SET_FIELD(gain_data, NPCX_ADC_AGC_0_AGC_TARGET, agc_target);

	apm_write_indirect_data(APM_ADC_AGC_REG, APM_INDIRECT_ADC_AGC_0_REG,
				gain_data);

	gain_data = 0;

	if (nois_gate_en)
		SET_BIT(gain_data, NPCX_ADC_AGC_1_NG_EN);
	else
		CLEAR_BIT(gain_data, NPCX_ADC_AGC_1_NG_EN);
	SET_FIELD(gain_data, NPCX_ADC_AGC_1_NG_THR, nois_gate_thold);
	SET_FIELD(gain_data, NPCX_ADC_AGC_1_HOLD, hold_time);

	apm_write_indirect_data(APM_ADC_AGC_REG, APM_INDIRECT_ADC_AGC_1_REG,
				gain_data);

	gain_data = 0;

	SET_FIELD(gain_data, NPCX_ADC_AGC_2_ATK, attack_time);
	SET_FIELD(gain_data, NPCX_ADC_AGC_2_DCY, decay_time);

	apm_write_indirect_data(APM_ADC_AGC_REG, APM_INDIRECT_ADC_AGC_2_REG,
				gain_data);

	gain_data = 0;

	SET_FIELD(gain_data, NPCX_ADC_AGC_3_AGC_MAX, gain_max);

	apm_write_indirect_data(APM_ADC_AGC_REG, APM_INDIRECT_ADC_AGC_3_REG,
				gain_data);

	gain_data = 0;

	SET_FIELD(gain_data, NPCX_ADC_AGC_4_AGC_MIN, gain_min);

	apm_write_indirect_data(APM_ADC_AGC_REG, APM_INDIRECT_ADC_AGC_4_REG,
				gain_data);

	return EC_SUCCESS;
}

/**
 * Sets APM mode (enables & disables APN sub modules accordingly
 * to the APM mode).
 *
 * @param apm_mode - APM mode, DEFAULT, DETECTION, RECORD or INDEPENDENT modes.
 * @return  None
 */
void apm_set_mode(enum wov_modes wov_mode)
{
	switch (wov_mode) {
	case WOV_MODE_OFF:
		apm_enable(0);
		apm_vad_enable(0);
		apm_enable_vad_interrupt(0);
		apm_dmic_enable(0);
		apm_adc_enable(0);
		apm_vad_adc_wakeup_enable(0);
		wov_apm_active(0);
		break;

	case WOV_MODE_VAD:
		apm_clear_vad_detected_bit();
		wov_apm_active(1);
		apm_enable(1);
		apm_vad_restart();
		apm_vad_enable(1);
		apm_enable_vad_interrupt(1);
		apm_set_vad_dmic_rate_l(apm_conf.vad_dmic_rate);
		apm_dmic_enable(1);
		apm_adc_enable(0);
		apm_vad_adc_wakeup_enable(1);
		break;

	case WOV_MODE_RAM:
		wov_apm_active(1);
		apm_enable(1);
		apm_vad_enable(0);
		apm_enable_vad_interrupt(0);
		apm_set_adc_dmic_config_l(apm_conf.adc_dmic_rate);
		apm_dmic_enable(1);
		apm_adc_enable(1);
		apm_vad_adc_wakeup_enable(0);
		break;

	case WOV_MODE_I2S:
		wov_apm_active(1);
		apm_enable(1);
		apm_vad_enable(0);
		apm_enable_vad_interrupt(0);
		apm_set_adc_dmic_config_l(apm_conf.adc_dmic_rate);
		apm_dmic_enable(1);
		apm_adc_enable(1);
		apm_vad_adc_wakeup_enable(0);
		break;

	case WOV_MODE_RAM_AND_I2S:
		wov_apm_active(1);
		apm_enable(1);
		apm_vad_enable(0);
		apm_enable_vad_interrupt(0);
		apm_set_adc_dmic_config_l(apm_conf.adc_dmic_rate);
		apm_dmic_enable(1);
		apm_adc_enable(1);
		apm_vad_adc_wakeup_enable(0);
		break;

	default:
		apm_set_vad_dmic_rate_l(APM_DMIC_RATE_1_0);
		apm_set_adc_dmic_config_l(APM_DMIC_RATE_1_0);
		apm_enable(0);
		apm_vad_enable(0);
		apm_enable_vad_interrupt(0);
		apm_dmic_enable(0);
		apm_adc_enable(0);
		apm_vad_adc_wakeup_enable(0);
		wov_apm_active(0);
		break;
	}

	apm_adc_set_freq(apm_adc_freq_val_2_code(wov_conf.sample_per_sec));
}

/**
 * Clears VAD detected bit in IFR register.
 *
 * @param   None
 * @return  None.
 */
void apm_clear_vad_detected_bit(void)
{
	apm_vad_enable(0);

	APM_CLEAR_VAD_INTERRUPT;

	apm_vad_enable(1);
}
