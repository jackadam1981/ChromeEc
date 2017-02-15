/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for MEC17XX */

#include "hooks.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "registers.h"
#include "util.h"


/*
 * PWMs that must remain active in low-power idle - 
 * PWM 0,1-8 are b[4,20:27] of MEC17XX_PCR_SLP_EN1
 * PWM 9 is b[31] of MEC17XX_PCR_SLP_EN3
 * PWM 10 - 11 are b[0:1] of MEC17XX_PCR_SLP_EN4
 * store 32-bit word with 
 * b[0:1] = PWM 10-11
 * b[4,20:27] = PWM 0, 1-8
 * b[31] = PWM 9
 */
static uint32_t pwm_keep_awake_mask;

const uint8_t pwm_slp_bitpos[12] = {
	4, 20, 21, 22, 23, 24, 25, 26, 27, 31, 0, 1
};

static uint32_t pwm_get_sleep_mask(int id)
{
	uint32_t bitpos = 32;

	if (id < 12) {
		bitpos = (uint32_t)pwm_slp_bitpos[ch];
	}

	return (1ul << bitpos);
}


void pwm_enable(enum pwm_channel ch, int enabled)
{
	int id = pwm_channels[ch].channel;
	uint32_t pwm_slp_mask;

	pwm_slp_mask = pwm_get_sleep_mask(id);

	if (enabled) {
		MEC17XX_PWM_CFG(id) |= 0x1;
		if (pwm_channels[ch].flags & PWM_CONFIG_DSLEEP)
			pwm_keep_awake_mask |= pwm_slp_mask;
	} else {
		MEC17XX_PWM_CFG(id) &= ~0x1;
		pwm_keep_awake_mask &= ~pwm_slp_mask;
	}
}

int pwm_get_enabled(enum pwm_channel ch)
{
	return MEC17XX_PWM_CFG(pwm_channels[ch].channel) & 0x1;
}

void pwm_set_duty(enum pwm_channel ch, int percent)
{
	int id = pwm_channels[ch].channel;

	if (percent < 0)
		percent = 0;
	else if (percent > 100)
		percent = 100;

	MEC17XX_PWM_ON(id) = percent;
	MEC17XX_PWM_OFF(id) = 100 - percent;
}

int pwm_get_duty(enum pwm_channel ch)
{
	return MEC17XX_PWM_ON(pwm_channels[ch].channel);
}

void pwm_keep_awake(void)
{
	if (pwm_keep_awake_mask) {
		/* b[4,20:27] */
		MEC17XX_PCR_SLP_EN1 &= ~(pwm_keep_awake_mask & 0x0ff00010ul);
		/* b[31] */
		MEC17XX_PCR_SLP_EN3 &= ~(pwm_keep_awake_mask & (1ul << 31));
		/* b[1:0] */
		MEC17XX_PCR_SLP_EN4 &= ~(pwm_keep_awake_mask & 0x03ul);
	} else {
		MEC17XX_PCR_SLOW_CLK_CTL &= 0xFFFFFC00;
	}
}


static void pwm_configure(int ch, int active_low, int clock_low)
{
	/*
	 * clock_low=0 selects the 48MHz Ring Oscillator source
	 * clock_low=1 selects the 100kHz_Clk source
	 */
	MEC17XX_PWM_CFG(ch) = (15 << 3) |    /* Pre-divider = 16 */
			      (active_low ? (1 << 2) : 0) |
			      (clock_low  ? (1 << 1) : 0);
}

static void pwm_init(void)
{
	int i;

	CPUTS("HOOK_INIT - call pwm_init");

	for (i = 0; i < PWM_CH_COUNT; ++i) {
		pwm_configure(pwm_channels[i].channel,
			      pwm_channels[i].flags & PWM_CONFIG_ACTIVE_LOW,
			      pwm_channels[i].flags & PWM_CONFIG_ALT_CLOCK);
		pwm_set_duty(i, 0);
	}
}
DECLARE_HOOK(HOOK_INIT, pwm_init, HOOK_PRIO_DEFAULT);
