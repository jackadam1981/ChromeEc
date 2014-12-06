/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for NUCMX.
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

#if !(DEBUG_PWM)
#undef cprints
#define cprints(...)
#endif

/* PWM clock source */
enum nucmx_pwm_source_clock {
    NUCMX_PWM_CLOCK_APB2_LFCLK  = 0,
    NUCMX_PWM_CLOCK_FX          = 1,
    NUCMX_PWM_CLOCK_FR          = 2,
    NUCMX_PWM_CLOCK_RESERVED    = 0x3,
    
    NUCMX_PWM_CLOCK_UNDEF       = 0xFF
};

/* PWM heartbeat mode */
enum nucmx_pwm_heartbeat_mode {
    NUCMX_PWM_HBM_NORMAL    = 0,
    NUCMX_PWM_HBM_25        = 1,
    NUCMX_PWM_HBM_50        = 2,
    NUCMX_PWM_HBM_100       = 3,
    
    NUCMX_PWM_HBM_UNDEF     = 0xFF
};

/* Global variables */
static int pwm_init_ch = 0;

/**
 * Preset PWM operation clock.
 *
 * @param   none
 * @return  none
 * @notes   changed when initial or HOOK_FREQ_CHANGE command
 */
void pwm_freq_changed (void)
{
    uint32_t prescaler_divider    = 0;

	if (pwm_init_ch == PWM_CH_FAN)
	{
    /* Using PWM Frequency and Resolution we calculate prescaler for input clock *//* (Benson_TBD_9) pwm_clock/pwm_freq/pwm_resolution not confirm */
#ifdef CONFIG_PWM_INPUT_LFCLK
    prescaler_divider = (uint32_t)(32768 / pwm_channels[pwm_init_ch].freq);
#else
    prescaler_divider = (uint32_t)(clock_get_apb2_freq() / pwm_channels[pwm_init_ch].freq);
#endif
    }
    else
        prescaler_divider = (uint32_t)(clock_get_apb2_freq() / pwm_channels[pwm_init_ch].freq);

    /* Set clock prescalre divider to ADC module*/
    if (prescaler_divider >= 1)
        prescaler_divider = prescaler_divider - 1;
    if (prescaler_divider > 0xFFFF)
        prescaler_divider = 0xFFFF;

    /* Configure computed prescaler and resolution */
    NUCMX_PRSC(pwm_channels[pwm_init_ch].channel) = (uint16_t)prescaler_divider;
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, pwm_freq_changed, HOOK_PRIO_DEFAULT);

/**
 * Set PWM enabled.
 *
 * @param   ch      operation channel
 * @param   enabled enabled flag
 * @return  none
 */
void pwm_enable(enum pwm_channel ch, int enabled)
{
    /* Start or close PWM module */
    if (enabled)
        SET_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_PWR);
    else
        CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_PWR);
}

/**
 * Check PWM enabled.
 *
 * @param   ch  operation channel
 * @return  enabled or not
 */
int pwm_get_enabled(enum pwm_channel ch)
{
	return IS_BIT_SET(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_PWR);
}

/**
 * Set PWM duty cycle.
 *
 * @param   ch      operation channel
 * @param   percent duty cycle percent
 * @return  none
 */
void pwm_set_duty(enum pwm_channel ch, int percent)
{
    uint32_t resolution = 0;
    uint16_t duty_cycle = 0;

    cprints(CC_PWM, "pwm0=%d", percent);
	/* Assume the fan control is active high and invert it ourselves */
	if (pwm_channels[ch].flags & PWM_CONFIG_ACTIVE_LOW)
        SET_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_INVP);
    else
        CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_INVP);

	if (percent < 0)
		percent = 0;
	else if (percent > 100) /* (Benson_TBD_14) if 100% make mft cannot get TCRB, you will need to change to 99% */
		percent = 100;
    cprints(CC_PWM, "pwm1duty=%d", percent);

    resolution = NUCMX_CTR(pwm_channels[ch].channel) + 1;
    duty_cycle = percent*resolution/100;
    cprints(CC_PWM, "freq=0x%x", pwm_channels[ch].freq);
    cprints(CC_PWM, "resolution=%d", resolution);
    cprints(CC_PWM, "duty_cycle=%d", duty_cycle);

	/* Set the duty cycle *//* (Benson_TBD_14) Always enable the fan channel or not */
	if (percent)
	{
        NUCMX_DCR(pwm_channels[ch].channel) = (duty_cycle - 1);
    	pwm_enable(ch, 1);
    }
    else
    {
        NUCMX_DCR(pwm_channels[ch].channel) = 0;
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
	/* Return percent */
	return (((NUCMX_DCR(pwm_channels[ch].channel) +1 ) * 100) / (NUCMX_CTR(pwm_channels[ch].channel) + 1));
}

/**
 * PWM configuration.
 *
 * @param ch                        operation channel
 * @return none
 */
void pwm_config (enum pwm_channel ch)
{
	pwm_init_ch = ch;

	/* Configure pins from GPIOs to PWM */
	if (ch == PWM_CH_FAN)
	    gpio_config_module(MODULE_PWM_FAN, 1);
	else
	    gpio_config_module(MODULE_PWM_KBLIGHT, 1);

    /* Disable PWM for module configuration */
    pwm_enable(ch, 0);

    /* Set PWM heartbeat mode is no heartbeat*/
    NUCMX_PWMCTL(pwm_channels[ch].channel) = (NUCMX_PWMCTL(pwm_channels[ch].channel)&(~(((1<<2)-1)<<NUCMX_PWMCTL_HB_DC_CTL)))|(NUCMX_PWM_HBM_NORMAL<<NUCMX_PWMCTL_HB_DC_CTL);

    /* Set PWM operation frequence */
    pwm_freq_changed();

    /* Set PWM cycle time */
    NUCMX_CTR(pwm_channels[ch].channel) = (pwm_channels[ch].cycle_pulses - 1);

	/* Set the duty cycle */
    NUCMX_DCR(pwm_channels[ch].channel) = 0;

    /* Set PWM polarity is normal*/
    CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_INVP);

    /* Set PWM open drain output is push-pull type*/
    CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTLEX_OD_OUT);

    /* Select default CLK or LFCLK clock input to PWM module */
    NUCMX_PWMCTLEX(pwm_channels[ch].channel) = (NUCMX_PWMCTLEX(pwm_channels[ch].channel)&(~(((1<<2)-1)<<NUCMX_PWMCTLEX_FCK_SEL)))|(NUCMX_PWM_CLOCK_APB2_LFCLK<<NUCMX_PWMCTLEX_FCK_SEL);
	if (ch == PWM_CH_FAN)
	{
#ifdef CONFIG_PWM_INPUT_LFCLK
    /* Select default LFCLK clock input to PWM module */
    SET_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_CKSEL);
#else
    /* Select default core clock input to PWM module */
    CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_CKSEL);
#endif
    }
    else
        /* Select default core clock input to PWM module */
        CLEAR_BIT(NUCMX_PWMCTL(pwm_channels[ch].channel), NUCMX_PWMCTL_CKSEL);
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

#ifdef CONFIG_PWM_DSLEEP

	/* Enable the PWM module and delay a few clocks */
	clock_enable_peripheral(CGC_OFFSET_PWM, CGC_PWM_MASK, CGC_MODE_ALL);

#else

	/* Enable the PWM module and delay a few clocks */
	clock_enable_peripheral(CGC_OFFSET_PWM, CGC_PWM_MASK,
			CGC_MODE_RUN | CGC_MODE_SLEEP);
#endif

	for (i = 0; i < PWM_CH_COUNT; i++)
		pwm_config(i);
}

/* The chip-specific fan module initializes before this. */
DECLARE_HOOK(HOOK_INIT, pwm_init, HOOK_PRIO_DEFAULT);
