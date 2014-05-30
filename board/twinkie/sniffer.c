/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "dma.h"
#include "gpio.h"
#include "hwtimer.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

#define RX_COUNT 1024

/* edge timing samples */
static uint16_t samples[RX_COUNT];

/* --- RX operation using comparator linked to timer --- */
/* RX is using COMP1 triggering TIM1 CH1 */
//#define DMAC_TIM_RX STM32_DMAC_CH2
#define DMAC_TIM_RX STM32_DMAC_CH6
#define TIM_CCR_IDX 1
#define TIM_CCR_CS  1
#define EXTI_COMP 21

/* Timer used for RX clocking */
#define TIM_RX TIM_CLOCK_PD_RX
/* Clock divider for RX edges timings (2.4Mhz counter from 48Mhz clock) */
#define RX_CLOCK_DIV (20 - 1)

static const struct dma_option dma_tim_option = {
	DMAC_TIM_RX, (void *)&STM32_TIM_CCRx(TIM_RX, TIM_CCR_IDX),
	STM32_DMA_CCR_MSIZE_16_BIT | STM32_DMA_CCR_PSIZE_16_BIT | STM32_DMA_CCR_CIRC,
};

#if 0
void rx_complete(void)
{
	/* stop stampling TIM2 */
	STM32_TIM_CR1(TIM_RX) &= ~1;
	/* stop DMA */
	dma_disable(DMAC_TIM_RX);
}

void comp_enable_monitoring(void)
{
	/* clear comparator external interrupt */
	STM32_EXTI_PR = 1 << EXTI_COMP;
	/* clean up older comparator event */
	task_clear_pending_irq(STM32_IRQ_COMP);
	/* re-enable comparator interrupt to detect packets */
	task_enable_irq(STM32_IRQ_COMP);
}

void comp_disable_monitoring(void)
{
	/* stop monitoring RX during sampling */
	task_disable_irq(STM32_IRQ_COMP);
	/* clear comparator external interrupt */
	STM32_EXTI_PR = 1 << EXTI_COMP;
}

/* detect an edge on the PD RX pin */
void comp_handler(void)
{
	/* ignore the comparator IRQ until we are done with current message */
	comp_disable_monitoring();
}
DECLARE_IRQ(STM32_IRQ_COMP, comp_handler, 1);
#endif

static volatile uint8_t seq;
void tim_dma_handler(void)
{
	stm32_dma_regs_t *dma = STM32_DMA1_REGS;
	uint32_t stat = dma->isr & (STM32_DMA_ISR_HTIF(DMAC_TIM_RX)
				  | STM32_DMA_ISR_TCIF(DMAC_TIM_RX));
	seq++;
	dma->ifcr |= STM32_DMA_ISR_ALL(DMAC_TIM_RX);
	task_set_event(TASK_ID_SNIFFER, TASK_EVENT_CUSTOM(stat), 0);
}
DECLARE_IRQ(STM32_IRQ_DMA_CHANNEL_4_7, tim_dma_handler, 1);

static void sniffer_init(void)
{
	return;

	/* remap TIM1 CH1/2/3 to DMA channel 6 */
	STM32_SYSCFG_CFGR1 |= 1 << 28;
	/* --- set counter for RX timing : 2.4Mhz rate, free-running --- */
	__hw_timer_enable_clock(TIM_RX, 1);
	/* Timer configuration */
	STM32_TIM_CR1(TIM_RX) = 0x0000;
	STM32_TIM_CR2(TIM_RX) = 0x0000;
	/* Auto-reload value : 16-bit free running counter */
	STM32_TIM_ARR(TIM_RX) = 0xFFFF;
	/* Counter reloading event after 27.3ms */
	STM32_TIM_CCR2(TIM_RX) = 0xFFFF;
	/* Timer ICx input configuration */
#if TIM_CCR_IDX == 1
	STM32_TIM_CCMR1(TIM_RX) = TIM_CCR_CS << 0;
#elif TIM_CCR_IDX == 4
	STM32_TIM_CCMR2(TIM_RX) = TIM_CCR_CS << 8;
#else
#error Unsupported RX timer capture input
#endif
	STM32_TIM_CCER(TIM_RX) = 0xB << ((TIM_CCR_IDX - 1) * 4);
	/* configure DMA request on CCRx update */
	STM32_TIM_DIER(TIM_RX) = (1 << (8 + TIM_CCR_IDX)) /* CCxDE */ | (1 << (8+2));
	/* set prescaler to /26 (F=2.4Mhz, T=0.4us) */
	STM32_TIM_PSC(TIM_RX) = RX_CLOCK_DIV;
	/* Reload the pre-scaler and reset the counter */
	STM32_TIM_EGR(TIM_RX) = 0x0001 | (1 << TIM_CCR_IDX) /* clear CCRx */;
	/* clear update event from reloading */
	STM32_TIM_SR(TIM_RX) = 0;

	/* --- DAC configuration for comparator at 550mV --- */
	/* Enable DAC interface clock. */
	STM32_RCC_APB1ENR |= (1 << 29);
	/* set voltage Vout=0.550V (Vref = 3.0V) */
	STM32_DAC_DHR12RD = 550 * 4096 / 3000;
	/* Start DAC channel 1 */
	STM32_DAC_CR = STM32_DAC_CR_EN1 | STM32_DAC_CR_BOFF1;

	/* --- COMP2 as comparator for RX vs Vmid = 550mV --- */
	/* turn on COMP/SYSCFG */
	STM32_RCC_APB2ENR |= 1 << 0;
	/* currently in hi-speed mode : INP = PA1 , INM = DAC1 / PA4 / INM4 */
	STM32_COMP_CSR = STM32_COMP_CMP1EN | STM32_COMP_CMP1MODE_HSPEED |
			 STM32_COMP_CMP1INSEL_VREF12 |
			 /*STM32_COMP_CMP1INSEL_INM4 |*/
			 STM32_COMP_CMP1OUTSEL_TIM1_IC1 |
			 STM32_COMP_CMP1HYST_HI;
#if 0
	/* comparator interrupt setup */
	STM32_EXTI_FTSR |= 1 << EXTI_COMP;
	STM32_EXTI_IMR |= 1 << EXTI_COMP;
	task_enable_irq(STM32_IRQ_COMP);
#endif
	ccprintf("Sniffer initialized\n");

	/* start sampling the edges on the CC line using the RX timer */
	dma_start_rx(&dma_tim_option, RX_COUNT, samples);
	{
	stm32_dma_chan_t *chan = dma_get_channel(DMAC_TIM_RX);
	chan->ccr |= STM32_DMA_CCR_TCIE | STM32_DMA_CCR_HTIE;
	task_enable_irq(STM32_IRQ_DMA_CHANNEL_4_7);
	}
	/* start RX timer */
	STM32_TIM_CR1(TIM_RX) |= 1;
}
DECLARE_HOOK(HOOK_INIT, sniffer_init, HOOK_PRIO_DEFAULT);

#define TX_BUF_NEXT(i) (((i) + 1) & (CONFIG_UART_TX_BUF_SIZE - 1))
extern volatile char tx_buf[CONFIG_UART_TX_BUF_SIZE];
extern volatile int tx_buf_head;
void sniffer_task(void)
{
	uint16_t prev = 0;
	uint16_t prev_t = 0;
	while (1) {
		uint32_t evt = task_wait_event(-1 /*TODO Purge on timeout*/);
		int off = evt & STM32_DMA_ISR_TCIF(DMAC_TIM_RX)
			  ? RX_COUNT/2 : 0;
		int i;
		uart_putc(seq);

		for (i = off; i < off + RX_COUNT/2 - 1; i+=2) {
			uint16_t t1 = samples[i] == prev ? 0  : samples[i];
			uint16_t t2 = samples[i+1] == samples[i] ? 0 : samples[i+1];
			uint16_t diff0 = t1 - prev_t;
			uint16_t diff1 = t2 - t1;
			prev = samples[i+1];
			prev_t = t2;
			if (diff0 >= 0xf || diff1 >= 0xf) {
				tx_buf[tx_buf_head] = 0xff;
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
				tx_buf[tx_buf_head] = diff0 & 0xff;
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
				tx_buf[tx_buf_head] = diff0 >> 8;
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
				tx_buf[tx_buf_head] = diff1 & 0xff;
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
				tx_buf[tx_buf_head] = diff1 >> 8;
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
			} else {
				tx_buf[tx_buf_head] = diff0 | (diff1 << 4);
				tx_buf_head = TX_BUF_NEXT(tx_buf_head);
			}
		}
		uart_putc(seq);
		uart_putc('\n');
	}
}

static int command_buff(int argc, char **argv)
{
	int bit;

	ccprintf("000:- ");
	/* Packet debug output */
	for (bit = 1; bit <  RX_COUNT; bit++) {
		uint16_t cnt = 	samples[bit] - samples[bit-1];
		if ((bit & 31) == 0)
			ccprintf("\n%03d:", bit);
		ccprintf("%1d ", cnt);
	}
	ccprintf("><\n");
	cflush();
	for (bit = 0; bit < RX_COUNT; bit++) {
		if ((bit & 31) == 0)
			ccprintf("\n%03d:", bit);
		ccprintf("%04x ", samples[bit]);
	}
	ccprintf("||\n");
	cflush();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(buff, command_buff,
                        "[]",
                        "Buffer content",
                        NULL);

static int command_ptr(int argc, char **argv)
{
	stm32_dma_chan_t *rx = dma_get_channel(DMAC_TIM_RX);

	ccprintf("DMA %d\n",dma_bytes_done(rx, RX_COUNT));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ptr, command_ptr,
                        "[]",
                        "Buffering status",
                        NULL);
