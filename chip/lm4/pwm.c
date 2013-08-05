/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for LM4 */

#include "clock.h"
#include "gpio.h"
#include "hooks.h"
#include "pwm.h"
#include "pwm_data.h"
#include "registers.h"
#include "thermal.h"
#include "util.h"

/* Maximum RPM for PWM controller */
#define MAX_RPM 0x1fff

/* Maximum PWM for PWM controller */
#define MAX_PWM 0x1ff

#define RPM_SCALE 2

void pwm_enable(enum pwm_channel ch, int enabled)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	if (enabled)
		LM4_FAN_FANCTL |= (1 << pwm->channel);
	else
		LM4_FAN_FANCTL &= ~(1 << pwm->channel);
}

int pwm_get_enabled(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	return (LM4_FAN_FANCTL & (1 << pwm->channel)) ? 1 : 0;
}

void pwm_set_duty(enum pwm_channel ch, int percent)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	int duty;

	if (percent < 0)
		percent = 0;
	else if (percent > 100)
		percent = 100;

	duty = (MAX_PWM * percent) / 100;

	/* Always enable the channel */
	pwm_enable(ch, 1);

	/* Set the duty cycle */
	LM4_FAN_FANCMD(pwm->channel) = duty << 16;
}

int pwm_get_duty(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;

	return (LM4_FAN_FANCMD(pwm->channel) >> 16) * 100 / MAX_PWM;
}

static void pwm_init(void)
{
	int i;
	const struct pwm_t *pwm;
	const struct gpio_info *pwm_pin;
	const struct gpio_info *tach_pin;

	/* Enable the fan module and delay a few clocks */
	LM4_SYSTEM_RCGCFAN = 1;
	clock_wait_cycles(3);

	/* Disable all fans */
	LM4_FAN_FANCTL = 0;

	for (i = 0; i < PWM_CH_COUNT; ++i) {
		pwm = pwm_channels + i;

		/* Configure GPIOs */
		pwm_pin = gpio_list + pwm->pwm_pin;
		gpio_set_alternate_function(pwm_pin->port, pwm_pin->mask,
					    pwm->pwm_alt_func);

		if (pwm->tach_pin != PWM_NO_TACH) {
			tach_pin = gpio_list + pwm->tach_pin;
			gpio_set_alternate_function(tach_pin->port,
						    tach_pin->mask,
						    pwm->tach_alt_func);
		} else {
			tach_pin = NULL;
		}

		/* Configure PWM */
		if (pwm->config & PWM_CONFIG_HAS_RPM_MODE)
			LM4_FAN_FANCH(pwm->channel) = 0x802c;
		else
			LM4_FAN_FANCH(pwm->channel) = 0x0001;
	}
}
DECLARE_HOOK(HOOK_INIT, pwm_init, HOOK_PRIO_DEFAULT);
