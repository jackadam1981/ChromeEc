/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "clock.h"
#include "dma.h"
#include "hwtimer.h"
#include "hooks.h"
#include "registers.h"
#include "dac_chip.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define DAC_CHANNEL 2

#if (DAC_CHANNEL == 1) /* DAC output channel 1 : DAC_OUT1 */
#define DAC_DMA_CH      STM32_DMAC_DAC_CH1
#define DAC_DHR         STM32_DAC_DHR8R1
#define DAC_CR_SETTINGS (STM32_DAC_CR_DMAEN1 | STM32_DAC_CR_TSEL1_TMR2 |\
			 STM32_DAC_CR_TEN1 | STM32_DAC_CR_BOFF1 |\
			 STM32_DAC_CR_EN1)
#else /* DAC output channel 2 : DAC_OUT2 */
#define DAC_DMA_CH STM32_DMAC_DAC_CH2
#define DAC_DHR    STM32_DAC_DHR8R2
#define DAC_CR_SETTINGS (STM32_DAC_CR_DMAEN2 | STM32_DAC_CR_TSEL2_TMR2 |\
			 STM32_DAC_CR_TEN2 | STM32_DAC_CR_BOFF2 |\
			 STM32_DAC_CR_EN2)
#endif

static const struct dma_option dma_dac_option = {
	DAC_DMA_CH, (void *)&DAC_DHR,
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_8_BIT,
};

int dac_play_samples(const uint8_t *data, int count, int sample_rate)
{
	int ret = EC_SUCCESS;
	stm32_dma_chan_t *chan = dma_get_channel(DAC_DMA_CH);

	/* Enable DAC interface clock. */
	STM32_RCC_APB1ENR |= (1 << 29);

	/* Queue samples */
	dma_prepare_tx(&dma_dac_option, count, data);

	/* Setup TIM2 to produce the sample rate */
	__hw_timer_enable_clock(2, 1);
	STM32_TIM_PSC(2) = 0;
	STM32_TIM_ARR(2) = (clock_get_freq() / sample_rate) - 1;
	STM32_TIM_CR1(2) = 1 << 4; /* down-counter */
	STM32_TIM_EGR(2) = 0x0001; /* reload the pre-scaler and the counter */
	STM32_TIM_CR2(2) = 2 << 4; /* Toggle TRGO on update */
	STM32_TIM_SMCR(2) = 0;

	/* Enable DMA channel */
	dma_go(chan);
	/* Start counting DAC samples */
	STM32_TIM_CR1(2) |= 1;

	/* Start DAC channel 1 */
	STM32_DAC_CR = DAC_CR_SETTINGS;

	if (dma_wait(DAC_DMA_CH))
		ret = EC_ERROR_UNKNOWN;

	/* Stop DAC */
	STM32_DAC_CR = 0;
	/* Stop timer */
	STM32_TIM_CR1(2) &= ~1;
	/* Reset DMA status */
	dma_clear_isr(DAC_DMA_CH);

	return ret;
}
