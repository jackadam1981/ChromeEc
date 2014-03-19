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

struct mutex adc_lock;

static int watchdog_ain_id;

static const struct dma_option dma_adc_option = {
	STM32_DMAC_ADC, (void *)&STM32_ADC_DR,
	STM32_DMA_CCR_MSIZE_32_BIT | STM32_DMA_CCR_PSIZE_32_BIT,
};

static uint8_t sorted_channel_id[ADC_CH_COUNT];
static int channel_sorted;

static void sort_channels(void)
{
	int last = -1;
	int i, j;
	int this_min, which = -1;
	const struct adc_t *adc;

	for (i = 0; i < ADC_CH_COUNT; ++i) {
		this_min = 100; /* Max channel ID is 18 */
		for (j = 0; j < ADC_CH_COUNT; ++j) {
			adc = adc_channels + j;
			if (adc->channel <= last)
				continue;
			if (adc->channel < this_min) {
				this_min = adc->channel;
				which = j;
			}
		}
		last = this_min;
		sorted_channel_id[which] = i;
	}
	channel_sorted = 1;
}

static void adc_configure(int ain_id)
{
	/* Select channel to convert */
	STM32_ADC_CHSELR = 1 << ain_id;

	/* Disable DMA */
	STM32_ADC_CFGR1 &= ~0x1;
}

static void adc_continuous_read(int ain_id)
{
	adc_configure(ain_id);

	/* CONT=1 */
	STM32_ADC_CFGR1 |= 1 << 13;

	STM32_ADC_CR |= 1 << 2 /* ADSTART */;
}

static void adc_continuous_stop(void)
{
	STM32_ADC_CR |= 1 << 4; /* ADSTP */
	while (STM32_ADC_CR & (1 << 4))
		;

	/* CONT=0 */
	STM32_ADC_CFGR1 &= ~(1 << 13);
}

static int adc_watchdog_enabled(void)
{
	return STM32_ADC_CFGR1 & (1 << 23);
}

static int adc_enable_watchdog_no_lock(void)
{
	/* Select channel */
	STM32_ADC_CFGR1 = (STM32_ADC_CFGR1 & ~0x7c000000) | (watchdog_ain_id << 26);
	adc_configure(watchdog_ain_id);

	/* Clear AWD interupt flag */
	STM32_ADC_ISR = 0x80;
	/* Set Watchdog enable bit on a single channel */
	STM32_ADC_CFGR1 |= (1 << 23) | (1 << 22);
	/* Enable interrupt */
	STM32_ADC_IER |= 1 << 7;

	adc_continuous_read(watchdog_ain_id);

	return EC_SUCCESS;
}

int adc_enable_watchdog(int ain_id, int high, int low)
{
	int ret;

	mutex_lock(&adc_lock);

	watchdog_ain_id = ain_id;

	/* Set thresholds */
	STM32_ADC_TR = ((high & 0xfff) << 16) | (low & 0xfff);

	ret = adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);
	return ret;
}

static int adc_disable_watchdog_no_lock(void)
{
	adc_continuous_stop();

	/* Clear Watchdog enable bit */
	STM32_ADC_CFGR1 &= ~(1 << 23);

	return EC_SUCCESS;
}

int adc_disable_watchdog(void)
{
	int ret;

	mutex_lock(&adc_lock);
	ret = adc_disable_watchdog_no_lock();
	mutex_unlock(&adc_lock);
	return ret;
}

int adc_read_channel(enum adc_channel ch)
{
	const struct adc_t *adc = adc_channels + ch;
	int value;
	int restore_watchdog = 0;

	mutex_lock(&adc_lock);
	if (adc_watchdog_enabled()) {
		restore_watchdog = 1;
		adc_disable_watchdog_no_lock();
	}

	adc_configure(adc->channel);

	/* Clear flags */
	STM32_ADC_ISR = 0xe;

	STM32_ADC_CR |= 1 << 2 /* ADSTART */;

	/* Wait for end of conversion */
	while (!(STM32_ADC_ISR & (1 << 2)))
		;
	/* read converted value */
	value = STM32_ADC_DR;

	if (restore_watchdog)
		adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);

	return value * adc->factor_mul / adc->factor_div + adc->shift;
}

int adc_read_all_channels(int *data)
{
	int i;
	uint32_t channels = 0;
	uint32_t raw_data[ADC_CH_COUNT];
	const struct adc_t *adc;
	int restore_watchdog = 0;
	int ret = EC_SUCCESS;

	if (!channel_sorted)
		sort_channels();

	mutex_lock(&adc_lock);

	if (adc_watchdog_enabled()) {
		restore_watchdog = 1;
		adc_disable_watchdog_no_lock();
	}

	/* Select all used channels */
	for (i = 0; i < ADC_CH_COUNT; ++i)
		channels |= 1 << adc_channels[i].channel;
	STM32_ADC_CHSELR = channels;

	/* Enable DMA */
	STM32_ADC_CFGR1 |= 0x1;

	dma_start_rx(&dma_adc_option, ADC_CH_COUNT, raw_data);

	/* Clear flags */
	STM32_ADC_ISR = 0xe;

	STM32_ADC_CR |= 1 << 2; /* ADSTART */

	if (dma_wait(STM32_DMAC_ADC)) {
		ret = EC_ERROR_UNKNOWN;
		goto fail; /* goto fail; goto fail; */
	}

	for (i = 0; i < ADC_CH_COUNT; ++i) {
		adc = adc_channels + i;
		data[i] = (raw_data[sorted_channel_id[i]] & 0xffff) *
			   adc->factor_mul / adc->factor_div +
			   adc->shift;
	}

fail:
	if (restore_watchdog)
		adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);
	return ret;
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

void adc_watchdog_interrupt(void)
{
	STM32_ADC_ISR = 0x80;
	ccprintf("awd!!\n");
}
DECLARE_IRQ(STM32_IRQ_ADC_COMP, adc_watchdog_interrupt, 2);

static int command_start(int argc, char **argv)
{
	task_enable_irq(STM32_IRQ_ADC_COMP);
	return adc_enable_watchdog(0, 1800, 0);
}
DECLARE_CONSOLE_COMMAND(startadc, command_start, NULL, NULL, NULL);

static int command_stop(int argc, char **argv)
{
	return adc_disable_watchdog();
}
DECLARE_CONSOLE_COMMAND(stopadc, command_stop, NULL, NULL, NULL);
