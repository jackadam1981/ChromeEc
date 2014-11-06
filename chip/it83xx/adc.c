/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* IT8380 ADC module for Chrome EC */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "console.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "asm.h"

/* globals */
static uint16_t adc_raw_data[ADC_CH_COUNT] = {-1};

/* Data structure of ADC channel control registers. */
const struct adc_ctrl_t adc_ctrl_regs[] = {
	{&IT83XX_ADC_VCH0CTL, &IT83XX_ADC_VCH0DATM, &IT83XX_ADC_VCH0DATL,
		&IT83XX_GPIO_GPCRI0},
	{&IT83XX_ADC_VCH1CTL, &IT83XX_ADC_VCH1DATM, &IT83XX_ADC_VCH1DATL,
		&IT83XX_GPIO_GPCRI1},
	{&IT83XX_ADC_VCH2CTL, &IT83XX_ADC_VCH2DATM, &IT83XX_ADC_VCH2DATL,
		&IT83XX_GPIO_GPCRI2},
	{&IT83XX_ADC_VCH3CTL, &IT83XX_ADC_VCH3DATM, &IT83XX_ADC_VCH3DATL,
		&IT83XX_GPIO_GPCRI3},
	{&IT83XX_ADC_VCH4CTL, &IT83XX_ADC_VCH4DATM, &IT83XX_ADC_VCH4DATL,
		&IT83XX_GPIO_GPCRI4},
	{&IT83XX_ADC_VCH5CTL, &IT83XX_ADC_VCH5DATM, &IT83XX_ADC_VCH5DATL,
		&IT83XX_GPIO_GPCRI5},
	{&IT83XX_ADC_VCH6CTL, &IT83XX_ADC_VCH6DATM, &IT83XX_ADC_VCH6DATL,
		&IT83XX_GPIO_GPCRI6},
	{&IT83XX_ADC_VCH7CTL, &IT83XX_ADC_VCH7DATM, &IT83XX_ADC_VCH7DATL,
		&IT83XX_GPIO_GPCRI7},
};

int adc_read_channel(enum adc_channel ch)
{
	/* voltage 0 ~ 3v = adc data register raw data 0 ~ 3FFh (10-bit ) */
	if (adc_raw_data[ch] > 0x3FF)
		return ADC_READ_ERROR;
	else
		return (adc_raw_data[ch] * adc_channels[ch].factor_mul /
			adc_channels[ch].factor_div + adc_channels[ch].shift);
}

int adc_read_all_channels(int *data)
{
	int index;

	for (index = 0; index < ADC_CH_COUNT; index++) {
		data[index] = adc_read_channel(index);
		if (data[index] == ADC_READ_ERROR)
			return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

/*
 * ADC analog accuracy initialization (only once after VSTBY power on)
 *
 * Write 1 to this bit and write 0 to this bit immediately once and
 * only once during the firmware initialization and do not write 1 again
 * after initialization since IT8380 takes much power consumption
 * if this bit is set as 1
 */
static void adc_accuracy_initialization(void)
{
	/* bit3 : start adc accuracy initialization */
	IT83XX_ADC_ADCSTS |= 0x08;
	/* short delay for adc accuracy initialization */
	nop();
	nop();
	nop();
	nop();

	/* bit3 : stop adc accuracy initialization */
	IT83XX_ADC_ADCSTS &= ~0x08;
}

/* ISR for ADC module */
static void adc_ec_interrupt(void)
{
	int index;
	int ch;

	/* W/C interrupt status of ADC */
	task_clear_pending_irq(IT83XX_IRQ_ADC);

	for (index = 0; index < ADC_CH_COUNT; index++) {
		ch = adc_channels[index].channel;

		/* data valid of adc channel[x] */
		if (IT83XX_ADC_ADCDVSTS & (1 << ch)) {
			/* read adc raw data msb and lsb */
			adc_raw_data[index] =
				(*adc_ctrl_regs[ch].adc_datm << 8) +
					*adc_ctrl_regs[ch].adc_datl;

			/* W/C data valid flag */
			IT83XX_ADC_ADCDVSTS = (1 << ch);
		}
	}
}
DECLARE_IRQ(IT83XX_IRQ_ADC, adc_ec_interrupt, 1);

/* ADC module Initialization */
static void adc_init(void)
{
	int index;
	int ch;

	/* ADC analog accuracy initialization */
	adc_accuracy_initialization();

	for (index = 0; index < ADC_CH_COUNT; index++) {
		ch = adc_channels[index].channel;

		/* enable adc channel[x] function pin */
		*adc_ctrl_regs[ch].adc_pin_ctrl = 0x00;

		/*
		 * bit4 ~ bit0 : indicates voltage channel[x]
		 *               input is selected for measurement
		 * bit 7 : WC data valid flag
		 */
		if (ch < 4)
			/* for channel 0, 1, 2, and 3 */
			*adc_ctrl_regs[ch].adc_ctrl = 0x80 + ch;
		else
			/*
			 * for channel 4, 5, 6, and 7
			 * bit4 : voltage channel enable/disable (ch 4~7 only)
			 * bit7 : WC data valid flag
			 */
			*adc_ctrl_regs[ch].adc_ctrl = 0x90;
	}

	/* bit2 ~ bit0 : default 0x02, SAR ADC 2 - 7 */
	IT83XX_ADC_ADCGCR |= 0x07;

	/* 1932h : bit3 ~ bit1 : interval time of SAR ADC 2 - 7 */
	IT83XX_ADC_ADCSAR |= 0x0E;

	/*
	 * bit 0 : adc module enable/disable
	 * bit 2 : interrupt from End-of-Cycle event enable
	 * bit 5 : ADCCTS0 = 1
	 */
	IT83XX_ADC_ADCCFG = 0x25;

	/* W/C interrupt status of ADC */
	task_clear_pending_irq(IT83XX_IRQ_ADC);

	/* enable interrupt of ADC */
	task_enable_irq(IT83XX_IRQ_ADC);
}
DECLARE_HOOK(HOOK_INIT, adc_init, HOOK_PRIO_DEFAULT);
