/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

int adc_enable_watchdog(int ain_id, int high, int low)
{
	/* Set thresholds */
	STM32_ADC_TR = ((high & 0xfff) << 16) | (low & 0xfff);
	/* Set Watchdog enable bit on a single channel */
	STM32_ADC_CFGR1 |= (1 << 23) | (1 << 22);

	return EC_SUCCESS;
}

int adc_disable_watchdog(void)
{
	/* Clear Watchdog enable bit */
	STM32_ADC_CFGR1 &= ~(1 << 23);

	return EC_ERROR_UNKNOWN;
}

int adc_read_channel(enum adc_channel ch)
{
	const struct adc_t *adc = adc_channels + ch;
	int value;

	/* Select channel to convert */
	STM32_ADC_CHSELR = 1 << adc->channel;

	/* Clear flags */
	STM32_ADC_ISR = 0xe;

	STM32_ADC_CR |= 1 << 2 /* ADSTART */;

	/* Wait for end of conversion */
	while (!(STM32_ADC_ISR & (1 << 2)))
		;
	/* read converted value */
	value = STM32_ADC_DR;

	return value * adc->factor_mul / adc->factor_div + adc->shift;
}

int adc_read_all_channels(int *data)
{
	int i;
	uint32_t channels = 0;

	/* Select all used channels */
	for (i = 0; i < ADC_CH_COUNT; ++i)
		channels |= 1 << adc_channels[i].channel;
	STM32_ADC_CHSELR = channels;

	/* TODO */

	return EC_ERROR_UNKNOWN;
}

static void adc_init(void)
{
	/* Enable ADC clock */
	STM32_RCC_APB2ENR |= (1 << 9);
	/* check HSI14 in RCC ? ON by default */

	/* ADC calibration (done with ADEN = 0) */
	STM32_ADC_CR = 1 << 31; /* set ADCAL = 1, ADC off */
	/* wait for the end of calibration */
	while (STM32_ADC_CR & (1 << 31))
		;

	/* ADC enabled */
	STM32_ADC_CR = 1 << 0;

	/* Single conversion, right aligned, 12-bit */
	STM32_ADC_CFGR1 = 1 << 12; /* (1 << 15) => AUTOOFF */;
	/* clock is ADCCLK */
	STM32_ADC_CFGR2 = 0;
	/* Sampling time : 13.5 ADC clock cycles. */
	STM32_ADC_SMPR = 2;
}
DECLARE_HOOK(HOOK_INIT, adc_init, HOOK_PRIO_DEFAULT);
