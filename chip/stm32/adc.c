/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "board.h"
#include "console.h"
#include "hooks.h"
#include "registers.h"
#include "stm32_adc.h"
#include "task.h"
#include "timer.h"
#include "util.h"

struct mutex adc_lock;

static void adc_configure(int ain_id)
{
	/* Set ADC to do a single conversion on only 1 channel
	 * TODO: Do we need multiple channels?
	 */
	STM32_ADC1_SQR1 = (0 << 20);  /* 1 conversion */
	STM32_ADC1_SQR3 = ain_id;
	STM32_ADC1_SMPR2 = 0; /* 1.5 cycles. Conversion takes 0.875 us. */
}

static inline int adc_powered(void)
{
	return STM32_ADC1_CR2 & (1 << 0);
}

static inline int adc_conversion_ended(void)
{
	return STM32_ADC1_SR & (1 << 1);
}

int adc_read_channel(enum adc_channel ch)
{
	const struct adc_t *adc = adc_channels + ch;
	int value;

	mutex_lock(&adc_lock);

	adc_configure(adc->channel);

	if (!adc_powered())
		return EC_ERROR_UNKNOWN;

	/* Clear EOC bit */
	STM32_ADC1_SR &= ~(1 << 1);

	/* Start conversion */
	STM32_ADC1_CR2 |= (1 << 0); /* ADON */

	/* Wait for EOC bit set */
	while (!adc_conversion_ended())
		;
	value = STM32_ADC1_DR & 0xfff;

	mutex_unlock(&adc_lock);

	return value * adc->factor_mul / adc->factor_div + adc->shift;
}

static int adc_init(void)
{
	/* Enable ADC clock */
	STM32_RCC_APB2ENR |= (1 << 9);

	if (!adc_powered()) {
		/* Power on ADC module */
		STM32_ADC1_CR2 |= (1 << 0);  /* ADON */

		/* Reset calibration */
		STM32_ADC1_CR2 |= (1 << 3);  /* RSTCAL */
		while (STM32_ADC1_CR2 & (1 << 3))
			;

		/* A/D Calibrate */
		STM32_ADC1_CR2 |= (1 << 2);  /* CAL */
		while (STM32_ADC1_CR2 & (1 << 2))
			;
	}

	/* Set right alignment */
	STM32_ADC1_CR2 &= ~(1 << 11);

	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_INIT, adc_init, HOOK_PRIO_DEFAULT);

static int command_adc(int argc, char **argv)
{
	int i;

	for (i = 0; i < ADC_CH_COUNT; ++i)
		ccprintf("ADC channel \"%s\" = %d\n",
			 adc_channels[i].name, adc_read_channel(i));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(adc, command_adc,
			NULL,
			"Print ADC channels",
			NULL);
