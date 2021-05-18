/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"

struct mutex adc_lock;

struct adc_profile_t {
	/* Register values. */
	uint32_t cfgr1_reg;
	uint32_t cfgr2_reg;
	uint32_t smpr_reg;	/* Default Sampling Rate */
	uint32_t ier_reg;
	/* DMA config. */
	const struct dma_option *dma_option;
	/* Size of DMA buffer, in units of ADC_CH_COUNT. */
	int dma_buffer_size;
};

#ifdef CONFIG_ADC_PROFILE_SINGLE
#ifndef CONFIG_ADC_SAMPLE_TIME
#define CONFIG_ADC_SAMPLE_TIME STM32_ADC_SMPR_12_5_CY
#endif
#endif

#if defined(CHIP_FAMILY_STM32L4)
#define ADC_CALIBRATION_TIMEOUT_US	1000U
#define ADC_ENABLE_TIMEOUT_US		1000U
#define ADC_CONVERSION_TIMEOUT_US	2000U

#define NUMBER_OF_ADC_CHANEEL   2
uint8_t adc1_initialed;
#endif

#ifdef CONFIG_ADC_PROFILE_FAST_CONTINUOUS

#ifndef CONFIG_ADC_SAMPLE_TIME
#define CONFIG_ADC_SAMPLE_TIME STM32_ADC_SMPR_1_5_CY
#endif

static const struct dma_option dma_continuous = {
	STM32_DMAC_ADC, (void *)&STM32_ADC_DR,
	STM32_DMA_CCR_MSIZE_32_BIT | STM32_DMA_CCR_PSIZE_32_BIT |
	STM32_DMA_CCR_CIRC,
};

static const struct adc_profile_t profile = {
	/* Sample all channels continuously using DMA */
	.cfgr1_reg = STM32_ADC_CFGR1_OVRMOD |
		     STM32_ADC_CFGR1_CONT |
		     STM32_ADC_CFGR1_DMACFG,
	.cfgr2_reg = 0,
	.smpr_reg = CONFIG_ADC_SAMPLE_TIME,
	/* Fire interrupt at end of sequence. */
	.ier_reg = STM32_ADC_IER_EOSEQIE,
	.dma_option = &dma_continuous,
	/* Double-buffer our samples. */
	.dma_buffer_size = 2,
};
#endif

static void adc_init(const struct adc_t *adc)
{
	/*
	 * If clock is already enabled, and ADC module is enabled
	 * then this is a warm reboot and ADC is already initialized.
	 */

	if (STM32_RCC_AHB2ENR & STM32_RCC_AHB2ENR_ADCEN &&
	    (STM32_ADC1_CR & STM32_ADC1_CR_ADEN))
		return;

	/* Enable ADC clock */
	clock_enable_module(MODULE_ADC, 1);
	/* check HSI14 in RCC ? ON by default */

	/* set ADC clock to 20MHz */
	STM32_ADC1_CCR &= ~0x003C0000;
	STM32_ADC1_CCR |=  0x00080000;
	
	STM32_RCC_AHB2ENR |= STM32_RCC_HB2_GPIOA;
	STM32_RCC_AHB2ENR |= STM32_RCC_HB2_GPIOB;

	/* Set ADC data resolution */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_CONT;
	/* Set ADC conversion data alignment */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_ALIGN;
	/* Set ADC delayed conversion mode */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_AUTDLY;
}

static void adc_configure(int ain_id, int ain_rank,
		enum stm32_adc_smpr sample_rate)
{
	/* Select Sampling time and channel to convert */
	if (ain_id <= 10)	{
		STM32_ADC1_SMPR1 &= ~(7 << ((ain_id - 1) * 3));
		STM32_ADC1_SMPR1 |= (sample_rate << ((ain_id - 1) * 3));
	} else	{
		STM32_ADC1_SMPR2 &= ~(7 << ((ain_id - 11) * 3));
		STM32_ADC1_SMPR2 |= (sample_rate << ((ain_id - 11) * 3));
	}

	/* Setup Rank */
	STM32_ADC1_JSQR &= ~(0x03);
	STM32_ADC1_JSQR |= NUMBER_OF_ADC_CHANEEL - 1;

	STM32_ADC1_JSQR &= ~(0x1F << (((ain_rank - 1) * 6) + 8));
	STM32_ADC1_JSQR |= (ain_id << (((ain_rank - 1) * 6) + 8));

	/* Disable DMA */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_DMAEN;
}

#ifdef CONFIG_ADC_WATCHDOG

static int watchdog_ain_id;
static int watchdog_delay_ms;

static void adc_continuous_read(int ain_id)
{
	adc_configure(ain_id, STM32_ADC_SMPR_DEFAULT);

	/* CONT=1 -> continuous mode on */
	STM32_ADC1_CFGR |= STM32_ADC1_CFGR_CONT;

	/* Start continuous conversion */
	STM32_ADC1_CR |= BIT(2); /* ADSTART */
}

static void adc_continuous_stop(void)
{
	/* Stop on-going conversion */
	STM32_ADC1_CR |= STM32_ADC1_CR_ADSTP;

	/* Wait for conversion to stop */
	while (STM32_ADC1_CR & STM32_ADC1_CR_ADSTP)
		;

	/* CONT=0 -> continuous mode off */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_CONT;
}

static void adc_interval_read(int ain_id, int interval_ms)
{
	adc_configure(ain_id, STM32_ADC1_SMPR_DEFAULT);

	/* EXTEN=01 -> hardware trigger detection on rising edge */
	STM32_ADC1_CFGR = (STM32_ADC1_CFGR & ~STM32_ADC_CFGR1_EXTEN_MASK)
				| STM32_ADC1_CFGR_EXTEN_RISE;

	/* EXTSEL=TRG3 -> Trigger on TIM3_TRGO */
	STM32_ADC1_CFGR = (STM32_ADC_CFGR1 & ~STM32_ADC1_CFGR_TRG_MASK) |
			STM32_ADC1_CFGR_TRG3;

	__hw_timer_enable_clock(TIM_ADC, 1);

	/* Upcounter, counter disabled, update event only on underflow */
	STM32_TIM_CR1(TIM_ADC) = 0x0004;

	/* TRGO on update event */
	STM32_TIM_CR2(TIM_ADC) = 0x0020;
	STM32_TIM_SMCR(TIM_ADC) = 0x0000;

	/* Auto-reload value */
	STM32_TIM_ARR(TIM_ADC) = interval_ms & 0xffff;

	/* Set prescaler to tick per millisecond */
	STM32_TIM_PSC(TIM_ADC) = (clock_get_freq() / MSEC) - 1;

	/* Start counting */
	STM32_TIM_CR1(TIM_ADC) |= 1;

	/* Start ADC conversion */
	STM32_ADC_CR |= BIT(2); /* ADSTART */
}

static void adc_interval_stop(void)
{
	/* EXTEN=00 -> hardware trigger detection disabled */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_EXTEN_MASK;

	/* Set ADSTP to clear ADSTART */
	STM32_ADC1_CR |= STM32_ADC1_CR_ADSTP;

	/* Wait for conversion to stop */
	while (STM32_ADC1_CR & STM32_ADC1_CR_ADSTP)
		;

	/* Stop the timer */
	STM32_TIM_CR1(TIM_ADC) &= ~0x1;
}

static int adc_watchdog_enabled(void)
{
	return STM32_ADC1_CFGR & STM32_ADC1_CFGR_AWDEN;
}

static int adc_enable_watchdog_no_lock(void)
{
	/* Select channel */
	STM32_ADC1_CFGR = (STM32_ADC1_CFGR & ~STM32_ADC1_CFGR_AWDCH_MASK) |
			  (watchdog_ain_id << 26);
	adc_configure(watchdog_ain_id, STM32_ADC_SMPR_DEFAULT);

	/* Clear AWD interrupt flag */
	STM32_ADC1_ISR = 0x80;
	/* Set Watchdog enable bit on a single channel */
	STM32_ADC1_CFGR |= STM32_ADC1_CFGR_AWDEN | STM32_ADC1_CFGR_AWDSGL;
	/* Enable interrupt */
	STM32_ADC1_IER |= STM32_ADC1_IER_AWDIE;

	if (watchdog_delay_ms)
		adc_interval_read(watchdog_ain_id, watchdog_delay_ms);
	else
		adc_continuous_read(watchdog_ain_id);

	return EC_SUCCESS;
}

int adc_enable_watchdog(int ain_id, int high, int low)
{
	int ret;

	mutex_lock(&adc_lock);

	watchdog_ain_id = ain_id;

	/* Set thresholds */
	STM32_ADC1_TR = ((high & 0xfff) << 16) | (low & 0xfff);

	ret = adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);
	return ret;
}

static int adc_disable_watchdog_no_lock(void)
{
	if (watchdog_delay_ms)
		adc_interval_stop();
	else
		adc_continuous_stop();

	/* Clear Watchdog enable bit */
	STM32_ADC1_CFGR &= ~STM32_ADC1_CFGR_AWDEN;

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

int adc_set_watchdog_delay(int delay_ms)
{
	int resume_watchdog = 0;

	mutex_lock(&adc_lock);
	if (adc_watchdog_enabled()) {
		resume_watchdog = 1;
		adc_disable_watchdog_no_lock();
	}

	watchdog_delay_ms = delay_ms;

	if (resume_watchdog)
		adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);

	return EC_SUCCESS;
}
#else /* CONFIG_ADC_WATCHDOG */

static int adc_watchdog_enabled(void) { return 0; }
static int adc_enable_watchdog_no_lock(void) { return 0; }
static int adc_disable_watchdog_no_lock(void) { return 0; }

#endif /* CONFIG_ADC_WATCHDOG */

int adc_read_channel(enum adc_channel ch)
{
	const struct adc_t *adc = adc_channels + ch;

	int value = 0;
	int restore_watchdog = 0;
	uint32_t wait_loop_index;

	mutex_lock(&adc_lock);

	if (adc1_initialed == 0) {
		adc_init(adc);

		/* Configure Injected Channel N */
		for (uint8_t i = 0; i < NUMBER_OF_ADC_CHANEEL; i++) {
			const struct adc_t *adc = adc_channels + i;

			adc_configure(adc->channel, adc->rank,
				      adc->sample_rate);
		}

		if ((STM32_ADC1_CR & STM32_ADC1_CR_ADEN) !=
		    STM32_ADC1_CR_ADEN) {
			/* Disable ADC deep power down (enabled by default after
			 * reset state)
			 */
			STM32_ADC1_CR &= ~STM32_ADC1_CR_DEEPPWD;
			/* Enable ADC internal voltage regulator */
			STM32_ADC1_CR |= STM32_ADC1_CR_ADVREGEN;
		}

		/* Delay for ADC internal voltage regulator stabilization.
		 * Compute number of CPU cycles to wait for, from delay in us.
		 *
		 * Note: Variable divided by 2 to compensate partially
		 * CPU processing cycles (depends on compilation optimization).
		 */
		/* Note: If system core clock frequency is below 200kHz, wait
		 * time
		 */
		/* is only a few CPU processing cycles. */
		wait_loop_index = ((20 * (80000000 / (100000 * 2))) / 10);
		while (wait_loop_index != 0)
			wait_loop_index--;

		/* Run ADC self calibration */
		STM32_ADC1_CR |= STM32_ADC1_CR_ADCAL;

		/* wait for the end of calibration */
		wait_loop_index = ((ADC_CALIBRATION_TIMEOUT_US *
				(CPU_CLOCK / (100000 * 2))) / 10);
		while (STM32_ADC1_CR & STM32_ADC1_CR_ADCAL) {
			wait_loop_index--;
			if (wait_loop_index == 0)
				break;
		}

		if (adc_watchdog_enabled()) {
			restore_watchdog = 1;
			adc_disable_watchdog_no_lock();
		}

		/* Enable ADC */
		wait_loop_index = ((ADC_ENABLE_TIMEOUT_US *
				(CPU_CLOCK / (100000 * 2))) / 10);
		STM32_ADC1_CR |= STM32_ADC1_CR_ADEN;
		while (!(STM32_ADC1_ISR & STM32_ADC1_ISR_ADRDY)) {
			wait_loop_index--;
			if (wait_loop_index == 0)
				break;
		}

		adc1_initialed = 1;
	}

	/* Start injected conversion */
	STM32_ADC1_CR |= BIT(3); /* JADSTART */

	/* Wait for end of injected conversion */
	wait_loop_index = ((ADC_CONVERSION_TIMEOUT_US *
			(CPU_CLOCK / (100000 * 2))) / 10);
	while (!(STM32_ADC1_ISR & BIT(6)))	{
		wait_loop_index--;
		if (wait_loop_index == 0)
			break;
	}

	/* Clear JEOS bit */
	STM32_ADC1_ISR |= BIT(6);

	/* read converted value */
	if (adc->rank == 1)
		value = STM32_ADC1_JDR1;
	if (adc->rank == 2)
		value = STM32_ADC1_JDR2;

	if (restore_watchdog)
		adc_enable_watchdog_no_lock();
	mutex_unlock(&adc_lock);

	return value * adc->factor_mul / adc->factor_div + adc->shift;
}

void adc_disable(void)
{
	/* Disable ADC */
	STM32_ADC1_CR &= ~STM32_ADC1_CR_ADEN;

	/* STM32_ADC1_CR |= STM32_ADC1_CR_ADDIS; */
	/*
	 * Note that the ADC is not in OFF state immediately.
	 * Once the ADC is effectively put into OFF state,
	 * STM32_ADC_CR_ADDIS bit will be cleared by hardware.
	 */
}
