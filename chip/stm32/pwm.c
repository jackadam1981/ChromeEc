/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PWM control module for STM32 */

#include "clock.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "pwm.h"
#include "pwm_data.h"
#include "registers.h"
#include "util.h"

static int using_pwm[PWM_CH_COUNT];

static volatile uint16_t * const stm32_tim_base[] = {
	NULL, /* So that index starts from 1 */
	(volatile uint16_t *)STM32_TIM1_BASE,
	(volatile uint16_t *)STM32_TIM2_BASE,
	(volatile uint16_t *)STM32_TIM3_BASE,
	(volatile uint16_t *)STM32_TIM4_BASE,
	(volatile uint16_t *)STM32_TIM5_BASE,
	(volatile uint16_t *)STM32_TIM6_BASE,
	(volatile uint16_t *)STM32_TIM7_BASE,
	(volatile uint16_t *)STM32_TIM8_BASE,
#ifdef CHIP_VARIANT_stm32f100
	NULL,
	NULL,
	NULL,
#else
	(volatile uint16_t *)STM32_TIM9_BASE,
	(volatile uint16_t *)STM32_TIM10_BASE,
	(volatile uint16_t *)STM32_TIM11_BASE,
#endif
	(volatile uint16_t *)STM32_TIM12_BASE,
	(volatile uint16_t *)STM32_TIM13_BASE,
	(volatile uint16_t *)STM32_TIM14_BASE,
	(volatile uint16_t *)STM32_TIM15_BASE,
	(volatile uint16_t *)STM32_TIM16_BASE,
	(volatile uint16_t *)STM32_TIM17_BASE,
};

static inline timer_ctlr_t *get_timer_ctlr(int timer_id)
{
	return (timer_ctlr_t *)stm32_tim_base[timer_id];
}

void pwm_set_duty(enum pwm_channel ch, int percent)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	timer_ctlr_t *tim = get_timer_ctlr(pwm->tim);

	ASSERT((percent >= 0) && (percent <= 100));
	tim->ccr[pwm->channel + 1] = percent;
}

int pwm_get_duty(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	timer_ctlr_t *tim = get_timer_ctlr(pwm->tim);
	return tim->ccr[pwm->channel + 1];
}

static void pwm_configure(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	const struct gpio_info *gpio = gpio_list + pwm->pin;
	timer_ctlr_t *tim = get_timer_ctlr(pwm->tim);
	volatile unsigned *ccmr = NULL;
#ifdef CHIP_FAMILY_stm32f
	int mask = gpio->mask;
	volatile uint32_t *gpio_cr = NULL;
	uint32_t val;
#endif

	if (using_pwm[ch])
		return;

#ifdef CHIP_FAMILY_stm32f
	if (mask < 0x100) {
		gpio_cr = &STM32_GPIO_CRL(gpio->port);
	} else {
		gpio_cr = &STM32_GPIO_CRH(gpio->port);
		mask >>= 8;
	}

	/* Expand mask from 8-bit to 32-bit */
	mask = mask * mask;
	mask = mask * mask;

	/* Set alternate function */
	val = *gpio_cr & ~(mask * 0xf);
	val |= mask * 0x9;
	*gpio_cr = val;
#else /* stm32l */
	gpio_set_alternate_function(gpio->port, gpio->mask,
				    GPIO_ALT_TIM(pwm->tim));
#endif

	/* Enable timer */
	__hw_timer_enable_clock(pwm->tim, 1);

	/* Disable counter during setup */
	tim->cr1 = 0x0000;

	/*
	 * CPU clock / PSC determines how fast the counter operates.
	 * ARR determines the wave period, CCRn determines duty cycle.
	 * Thus, frequency = cpu_freq / PSC / ARR. so:
	 *
	 *     frequency = cpu_freq / (cpu_freq/10000) / 100 = 100 Hz.
	 */
	tim->psc = clock_get_freq() / 10000;
	tim->arr = 100;

	if (pwm->channel < 2) /* Channel ID starts from 1 */
		ccmr = &tim->ccmr1;
	else
		ccmr = &tim->ccmr2;

	/* Output, PWM mode 1, preload enable */
	if (pwm->channel & 0x1)
		*ccmr = (6 << 12) | (1 << 11);
	else
		*ccmr = (6 << 4) | (1 << 3);

	/* Output enable. Set active high/low. */
	if (pwm->config & PWM_CONFIG_HAS_RPM_MODE)
		tim->ccer = 3 << (pwm->channel * 4);
	else
		tim->ccer = 1 << (pwm->channel * 4);

	/* Generate update event to force loading of shadow registers */
	tim->egr |= 1;

	/* Enable auto-reload preload, start counting */
	tim->cr1 |= (1 << 7) | (1 << 0);

	using_pwm[ch] = 1;
}

static void pwm_disable(enum pwm_channel ch)
{
	const struct pwm_t *pwm = pwm_channels + ch;
	timer_ctlr_t *tim = get_timer_ctlr(pwm->tim);

	if (using_pwm[ch] == 0)
		return;

	/* Disable counter */
	tim->cr1 &= ~0x1;

	/* Disable timer clock */
	__hw_timer_enable_clock(pwm->tim, 0);

	using_pwm[ch] = 0;
}

void pwm_enable(enum pwm_channel ch, int enabled)
{
	if (enabled)
		pwm_configure(ch);
	else
		pwm_disable(ch);
}

static void pwm_reconfigure(enum pwm_channel ch)
{
	using_pwm[ch] = 0;
	pwm_configure(ch);
}

/**
 * Handle clock frequency change
 */
static void pwm_freq_change(void)
{
	int i;
	for (i = 0; i < PWM_CH_COUNT; ++i)
		if (using_pwm[i])
			pwm_reconfigure(i);
}
DECLARE_HOOK(HOOK_FREQ_CHANGE, pwm_freq_change, HOOK_PRIO_DEFAULT);
