/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for NPCX.
 *
 * On this chip, the PWM logic is implemented by the hardware FAN modules.
 */

#include "clock.h"
#include "clock_chip.h"
#include "fan.h"
#include "gpio.h"
#include "hooks.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "util.h"
#include "console.h"
#include "assert.h"

#if !(DEBUG_PWM)
#define CPRINTS(...)
#else
#define CPRINTS(format, args...) cprints(CC_PWM, format, ## args)
#endif

/* PWM clock source */
enum npcx_pwm_source_clock {
	NPCX_PWM_CLOCK_APB2_LFCLK  = 0,
	NPCX_PWM_CLOCK_FX          = 1,
	NPCX_PWM_CLOCK_FR          = 2,
	NPCX_PWM_CLOCK_RESERVED    = 3,
	NPCX_PWM_CLOCK_UNDEF       = 0xFF
};

/* PWM heartbeat mode */
enum npcx_pwm_heartbeat_mode {
	NPCX_PWM_HBM_NORMAL    = 0,
	NPCX_PWM_HBM_25        = 1,
	NPCX_PWM_HBM_50        = 2,
	NPCX_PWM_HBM_100       = 3,
	NPCX_PWM_HBM_UNDEF     = 0xFF
};

#ifdef CONFIG_USE_16BIT_DUTY_CYCLE
/* MAX DUTY CYCLE VALUE */
#define MAX_DUTY_CYCLE 0xFFFF
#else
/* Default duty cycle resolution */
#define DUTY_CYCLE_RESOLUTION 100
#endif

#ifdef CONFIG_USE_16BIT_DUTY_CYCLE
/**
 * Set PWM operation clock.
 *
 * @param   ch      operation channel
 * @param   freq    desired PWM frequency
 * @notes   changed when initialization
 */
static void pwm_set_freq(enum pwm_channel ch, uint32_t freq)
{
	int mdl = pwm_channels[ch].channel;
	uint32_t prescaler_divider;
	uint32_t clock;

	assert(freq > 0);

	/* Disable PWM for module configuration */
	pwm_enable(ch, 0);

	/*
	 * Get PWM clock frequency. Use internal 32K as PWM clock source if
	 * the PWM must be active during low-power idle.
	 */

	if (pwm_channels[ch].flags & PWM_CONFIG_DSLEEP)
		clock = INT_32K_CLOCK;
	else
		clock = clock_get_apb2_freq();

	assert(clock > 0);

	/*
	 * Based on freq = clock / ((ctr + 1) * (prsc + 1))
	 *   where:  prsc = prescaler_divider
	 *           ctr  = MAX_DUTEY_CYCLE
	 */
	prescaler_divider = (clock / ((MAX_DUTY_CYCLE + 1) * freq)) - 1;

	/* Configure computed prescaler and resolution */
	NPCX_PRSC(mdl) = (uint16_t)prescaler_divider;

	/* Set PWM cycle time */
	NPCX_CTR(mdl) = MAX_DUTY_CYCLE;

	/* Set the duty cycle to 100% since DCR == CTR */
	NPCX_DCR(mdl) = MAX_DUTY_CYCLE;
}
#else
/**
 * Set PWM operation clock.
 *
 * @param   ch      operation channel
 * @param   freq    desired PWM frequency
 * @param   res     resolution for duty cycle
 * @notes   changed when initialization
 */
void pwm_set_freq(enum pwm_channel ch, uint32_t freq, uint32_t res)
{
	int mdl = pwm_channels[ch].channel;
	uint32_t prescaler_divider = 0;
	uint32_t clock;

	/* Disable PWM for module configuration */
	pwm_enable(ch, 0);

	/*
	 * Get PWM clock frequency. Use internal 32K as PWM clock source if
	 * the PWM must be active during low-power idle.
	 */

	if (pwm_channels[ch].flags & PWM_CONFIG_DSLEEP)
		clock = INT_32K_CLOCK;
	else
		clock = clock_get_apb2_freq();

	/*
	 * Using PWM Frequency and Resolution we calculate
	 * prescaler for input clock
	 */
	prescaler_divider = ((clock / freq)/res);

	/* Set clock prescaler divider to PWM module*/
	if (prescaler_divider >= 1)
		prescaler_divider = prescaler_divider - 1;
	if (prescaler_divider > 0xFFFF)
		prescaler_divider = 0xFFFF;

	/* Configure computed prescaler and resolution */
	NPCX_PRSC(mdl) = (uint16_t)prescaler_divider;

	/* Set PWM cycle time */
	NPCX_CTR(mdl) = res - 1;

	/* Set the duty cycle to 0% since DCR > CTR */
	NPCX_DCR(mdl) = res;
}
#endif
/**
 * Set PWM enabled.
 *
 * @param   ch      operation channel
 * @param   enabled enabled flag
 * @return  none
 */
void pwm_enable(enum pwm_channel ch, int enabled)
{
	int mdl = pwm_channels[ch].channel;

	/* Start or close PWM module */
	UPDATE_BIT(NPCX_PWMCTL(mdl), NPCX_PWMCTL_PWR, enabled);
}

/**
 * Check PWM enabled.
 *
 * @param   ch  operation channel
 * @return  enabled or not
 */
int pwm_get_enabled(enum pwm_channel ch)
{
	int mdl = pwm_channels[ch].channel;
	return IS_BIT_SET(NPCX_PWMCTL(mdl), NPCX_PWMCTL_PWR);
}

#ifdef CONFIG_USE_16BIT_DUTY_CYCLE
/**
 * Set PWM duty cycle.
 *
 * @param   ch      operation channel
 * @param   duty    cycle duty
 * @return  none
 */
void pwm_set_raw_duty(enum pwm_channel ch, uint32_t duty)
{
	int mdl = pwm_channels[ch].channel;

	CPRINTS("pwm%d, set duty=%d", mdl, duty);

	/* Assume the fan control is active high and invert it ourselves */
	UPDATE_BIT(NPCX_PWMCTL(mdl), NPCX_PWMCTL_INVP,
			(pwm_channels[ch].flags & PWM_CONFIG_ACTIVE_LOW));

	CPRINTS("freq=0x%x", pwm_channels[ch].freq);
	CPRINTS("duty_cycle_cnt=%d", duty);

	/* Set the duty cycle */
	NPCX_DCR(mdl) = (uint16_t)duty;

	if (duty > 0)
		pwm_enable(ch, 1);
	else
		pwm_enable(ch, 0);
}

/**
 * Get PWM duty cycle.
 *
 * @param   ch  operation channel
 * @return  duty cycle
 */
uint32_t pwm_get_raw_duty(enum pwm_channel ch)
{
	int mdl = pwm_channels[ch].channel;

	/* Return duty */
	if (!pwm_get_enabled(ch))
		return 0;
	else
		return NPCX_DCR(mdl);
}

#else

/**
 * Set PWM duty cycle.
 *
 * @param   ch      operation channel
 * @param   percent duty cycle percent
 * @return  none
 */
void pwm_set_duty(enum pwm_channel ch, int percent)
{
	int mdl = pwm_channels[ch].channel;
	uint32_t dc_res = 0;
	uint16_t dc_cnt = 0;

	/* Checking duty value first */
	if (percent < 0)
		percent = 0;
	else if (percent > 100)
		percent = 100;
	CPRINTS("pwm%d, set duty=%d", mdl, percent);

	/* Assume the fan control is active high and invert it ourselves */
	UPDATE_BIT(NPCX_PWMCTL(mdl), NPCX_PWMCTL_INVP,
			(pwm_channels[ch].flags & PWM_CONFIG_ACTIVE_LOW));

	dc_res = NPCX_CTR(mdl) + 1;
	dc_cnt = (percent*dc_res)/100;
	CPRINTS("freq=0x%x", pwm_channels[ch].freq);
	CPRINTS("duty_cycle_res=%d", dc_res);
	CPRINTS("duty_cycle_cnt=%d", dc_cnt);


	/* Set the duty cycle */
	if (percent > 0) {
		if (percent == 100)
			NPCX_DCR(mdl) = NPCX_CTR(mdl);
		else
			NPCX_DCR(mdl) = (dc_cnt - 1);
		pwm_enable(ch, 1);
	} else {
		/* Output low since DCR > CTR */
		NPCX_DCR(mdl) = NPCX_CTR(mdl) + 1;
		pwm_enable(ch, 0);
	}
}

/**
 * Get PWM duty cycle.
 *
 * @param   ch  operation channel
 * @return  duty cycle percent
 */
int pwm_get_duty(enum pwm_channel ch)
{
	int mdl = pwm_channels[ch].channel;
	/* Return percent */
	if ((!pwm_get_enabled(ch)) || (NPCX_DCR(mdl) > NPCX_CTR(mdl)))
		return 0;
	else
		return ((NPCX_DCR(mdl) + 1) * 100) / (NPCX_CTR(mdl) + 1);
}

#endif

/**
 * PWM configuration.
 *
 * @param  ch    operation channel
 * @return none
 */
void pwm_config(enum pwm_channel ch)
{
	int mdl = pwm_channels[ch].channel;

	/* Disable PWM for module configuration */
	pwm_enable(ch, 0);

	/* Set PWM heartbeat mode is no heartbeat */
	SET_FIELD(NPCX_PWMCTL(mdl), NPCX_PWMCTL_HB_DC_CTL_FIELD,
			NPCX_PWM_HBM_NORMAL);

	/* Select default CLK or LFCLK clock input to PWM module */
	SET_FIELD(NPCX_PWMCTLEX(mdl), NPCX_PWMCTLEX_FCK_SEL_FIELD,
			NPCX_PWM_CLOCK_APB2_LFCLK);

	/* Set PWM polarity normal first */
	CLEAR_BIT(NPCX_PWMCTL(mdl), NPCX_PWMCTL_INVP);

	/* Select PWM clock source */
	UPDATE_BIT(NPCX_PWMCTL(mdl), NPCX_PWMCTL_CKSEL,
			(pwm_channels[ch].flags & PWM_CONFIG_DSLEEP));

	/* Set PWM operation frequency */
#ifdef CONFIG_USE_16BIT_DUTY_CYCLE
	pwm_set_freq(ch, pwm_channels[ch].freq);
#else
	pwm_set_freq(ch, pwm_channels[ch].freq, DUTY_CYCLE_RESOLUTION);
#endif
}

/**
 * PWM initial.
 *
 * @param none
 * @return none
 */
static void pwm_init(void)
{
	int i;
	uint8_t pd_mask = 0;

	/* Take enabled PWMs out of power-down state */
	for (i = 0; i < PWM_CH_COUNT; i++)
		pd_mask |= (1 << pwm_channels[i].channel);
	clock_enable_peripheral(CGC_OFFSET_PWM, pd_mask, CGC_MODE_ALL);

	for (i = 0; i < PWM_CH_COUNT; i++)
		pwm_config(i);
}

/* The chip-specific fan module initializes before this. */
DECLARE_HOOK(HOOK_INIT, pwm_init, HOOK_PRIO_INIT_PWM);
