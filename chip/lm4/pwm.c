/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for LM4 */

#include "clock.h"
#include "gpio.h"
#include "hooks.h"
#include "lm4_pwm.h"
#include "pwm.h"
#include "registers.h"
#include "thermal.h"
#include "util.h"

/* Maximum RPM for PWM controller */
#define MAX_RPM 0x1fff

/* Maximum PWM for PWM controller */
#define MAX_PWM 0x1ff

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

	/* Move the fan to manual control */
	pwm_set_rpm_mode(ch, 0);

	/* Always enable the fan */
	pwm_enable(ch, 1);

#ifdef HAS_TASK_THERMAL
	/*
	 * Disable thermal engine automatic fan control.
	 * TODO: move this to where it belongs.
	 */
	if (ch == PWM_CH_FAN)
		thermal_control_fan(0);
#endif

	/* Set the duty cycle */
	LM4_FAN_FANCMD(pwm->channel) = duty << 16;
}

int pwm_get_duty(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;

	return (LM4_FAN_FANCMD(pwm->channel) >> 16) * 100 / MAX_PWM;
}

void pwm_set_target_rpm(enum pwm_channel ch, int rpm)
{
	const struct pwm_t *pwm = pwm_channels + ch;

	/* Apply RPM scaling */
	if (rpm > 0)
		rpm /= pwm->rpm_scale;

	/* Treat out-of-range requests as requests for maximum RPM */
	if (rpm < 0 || rpm > MAX_RPM)
		rpm = MAX_RPM;

	LM4_FAN_FANCMD(pwm->channel) = rpm;
}

int pwm_get_target_rpm(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	return (LM4_FAN_FANCMD(pwm->channel) & MAX_RPM) * pwm->rpm_scale;
}

int pwm_get_rpm(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	return (LM4_FAN_FANCST(pwm->channel) & MAX_RPM) * pwm->rpm_scale;
}

void pwm_set_rpm_mode(enum pwm_channel ch, int enabled)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	int was_enabled = pwm_get_enabled(ch);
	int was_rpm = pwm_get_rpm_mode(ch);

	if (!was_rpm && enabled) {
		/* Enable RPM control */
		pwm_enable(ch, 0);
		LM4_FAN_FANCH(pwm->channel) &= ~0x0001;
		pwm_enable(ch, was_enabled);
	} else if (was_rpm && !enabled) {
		/* Disable RPM control */
		pwm_enable(ch, 0);
		LM4_FAN_FANCH(pwm->channel) |= 0x0001;
		pwm_enable(ch, was_enabled);
	}
}

int pwm_get_rpm_mode(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	return (LM4_FAN_FANCH(pwm->channel) & 0x0001) ? 0 : 1;
}

int pwm_is_stalled(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;

	/* Must be enabled with non-zero target to stall */
	if (!pwm_get_enabled(ch) || pwm_get_target_rpm(ch) == 0)
		return 0;

	/* Check for stall condition */
	return (((LM4_FAN_FANSTS >> (2 * pwm->channel)) & 0x03) == 0) ? 1 : 0;
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
		LM4_FAN_FANCH(pwm->channel) = pwm->config;
	}
}
DECLARE_HOOK(HOOK_INIT, pwm_init, HOOK_PRIO_DEFAULT);
