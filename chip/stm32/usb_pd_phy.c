/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "clock.h"
#include "common.h"
#include "console.h"
#include "crc.h"
#include "dma.h"
#include "gpio.h"
#include "hwtimer.h"
#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

#define PD_DATARATE 300000 /* Hz */

/*
 * Maximum size of a Power Delivery packet (in bits on the wire) :
 *    16-bit header + 0..7 32-bit data objects  (+ 4b5b encoding)
 * 64-bit preamble + SOP (4x 5b) + message in 4b5b encoding + 32-bit CRC + EOP (1x 5b)
 * = 64 + 4*5 + 16 * 5/4 + 7 * 32 * 5/4 + 32 * 5/4 + 5
 */
#define PD_BIT_LEN 429

#define PD_MAX_RAW_SIZE (PD_BIT_LEN*2)

/* maximum number of consecutive similar bits with Biphase Mark Coding */
#define MAX_BITS 2

/* alternating bit sequence used for packet preamble : 00 10 11 01 00 ..  */
#define PD_PREAMBLE 0xB4B4B4B4 /* starts with 0, ends with 1 */

#define TX_CLOCK_DIV ((clock_get_freq() / (2*PD_DATARATE)))
#define RX_CLOCK_DIV (13 - 1)

/* threshold for 1 300-khz period */
#define PERIOD 4
#define NB_PERIOD(from,to) (((to) - (from) + (PERIOD/2)) & 0xFF) / PERIOD;
#define PERIOD_THRESHOLD ((PERIOD + 2*PERIOD) / 2)

/* Timers used for TX and RX clocking */
#define TIM_TX TIM_CLOCK_PD_TX
#define TIM_RX TIM_CLOCK_PD_RX

/* Board specific configuration */
#if defined(BOARD_FRUITPIE)
/* using SPI2 on PB12-14 */
#define SPI_REGS STM32_SPI2_REGS
static inline void spi_enable_clock(void)
{ STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2; }
static inline void spi_set_pins_speed(void)
{ STM32_GPIO_OSPEEDR(GPIO_B) |= 0x7f000000; }
#elif defined(BOARD_DISCOVERY)
/* using SPI1 on PA4-7 */
#define SPI_REGS STM32_SPI1_REGS
static inline void spi_enable_clock(void)
{ STM32_RCC_APB2ENR |= STM32_RCC_PB2_SPI1; }
static inline void spi_set_pins_speed(void)
{ STM32_GPIO_OSPEEDR(GPIO_A) |= 0xff00; }
#else
#error Board not supported
#endif

/* samples for the PD messages */
static uint32_t raw_samples[DIV_ROUND_UP(PD_MAX_RAW_SIZE, sizeof(uint32_t))];

/* state of the bit decoder */
static int d_toggle;
static int d_lastlen;
static uint32_t d_last;

void *pd_init_dequeue(void)
{
	/* preamble ends with 1 */
	d_toggle = 0;
	d_last = 0;
	d_lastlen = 0;

	return raw_samples;
}

static int wait_bits(int nb)
{
	int avail;
	stm32_dma_chan_t *rx = dma_get_channel(STM32_DMAC_CH7);

	avail = dma_bytes_done(rx, PD_MAX_RAW_SIZE);
	if (avail < nb) { /* no received yet ... */
		timestamp_t deadline = get_time();
		deadline.val += 4 * MAX_BITS * (nb - avail);
		while ((dma_bytes_done(rx, PD_MAX_RAW_SIZE) < nb)
			&& get_time().val < deadline.val)
			; /* optimized for latency, not CPU usage ... */
		if (dma_bytes_done(rx, PD_MAX_RAW_SIZE) < nb) {
			CPRINTF("TMOUT RX %d/%d\n",
				dma_bytes_done(rx, PD_MAX_RAW_SIZE), nb);
			return -1;
		}
	}
	return nb;
}

int pd_dequeue_bits(void *ctxt, int off, int len, uint32_t *val)
{
	int w;
	uint8_t cnt = 0xff;
	uint8_t *samples = ctxt;

	while ((d_lastlen < len) && (off < PD_MAX_RAW_SIZE - 1)) {
		w = wait_bits(off + 2);
		if (w < 0)
			goto stream_err;
		cnt = samples[off] - samples[off-1];
		if (!cnt || (cnt > 3*PERIOD))
			goto stream_err;
		off++;
		if (cnt <= PERIOD_THRESHOLD) {
			//w = wait_bits(off + 1);
			//if (w < 0)
			//	goto stream_err;
			cnt = samples[off] - samples[off-1];
			if (cnt >  PERIOD_THRESHOLD)
				goto stream_err;
			off++;
		}

		/* enqueue the bit of the last period */
		d_last = (d_last >> 1) | (cnt <= PERIOD_THRESHOLD ? 0x80000000 : 0);
		d_lastlen ++;
	}
	if (off < PD_MAX_RAW_SIZE) {
		*val = (d_last << (d_lastlen - len)) >> (32 - len);
		d_lastlen -= len;
		return off;
	} else {
		return -1;
	}
stream_err:
	CPRINTF("Invalid %d @%d\n",cnt, off);
	return -1;
}

int pd_find_preamble(void *ctxt)
{
	int bit;
	uint8_t *vals = ctxt;

	/*
	 * Detect preamble
	 * Alternate 1-period 1-period & 2-period.
	 */
	uint32_t all = 0;
	stm32_dma_chan_t *rx = dma_get_channel(STM32_DMAC_CH7);

	for (bit = 1; bit < PD_MAX_RAW_SIZE - 1; bit++) {
		uint8_t cnt;
		/* wait if the bit is not received yet ... */
		if (PD_MAX_RAW_SIZE - rx->cndtr - 1 < bit) {
			uint64_t timeout = get_time().val
					 + 4*(PD_MAX_RAW_SIZE - rx->cndtr - 1);
			while ((PD_MAX_RAW_SIZE - rx->cndtr - 1 < bit) &&
				(get_time().val < timeout))
				;
			if (PD_MAX_RAW_SIZE - rx->cndtr - 1 < bit) {
				CPRINTF("TMOUT RX %d/%d\n",
					PD_MAX_RAW_SIZE - rx->cndtr, bit);
				return -1;
			}
		}
		cnt = vals[bit] - vals[bit-1];
		all = (all >> 1) | (cnt <= PERIOD_THRESHOLD ? 1 << 31 : 0);
		if (all == 0x36db6db6)
			return bit - 1; /* should be SYNC-1 */
	}
	return -1;
}

static int b_toggle;

int pd_write_preamble(void *ctxt)
{
	uint32_t *msg = ctxt;

	/* 64-bit x2 preamble */
	msg[0] = PD_PREAMBLE;
	msg[1] = PD_PREAMBLE;
	msg[2] = PD_PREAMBLE;
	msg[3] = PD_PREAMBLE;
	b_toggle = 0x3FF; /* preamble ends with 1 */
	return 2*64;
}

int pd_write_sym(void *ctxt, int bit_off, uint32_t val10)
{
	uint32_t *msg = ctxt;
	int word_idx = bit_off / 32;
	int bit_idx = bit_off % 32;
	uint32_t val = b_toggle ^ val10;
	b_toggle = val & 0x200 ? 0x3FF : 0;
	if (bit_idx <= 22) {
		if (bit_idx == 0)
			msg[word_idx] = 0;
		msg[word_idx] |= val << bit_idx;
	} else {
		msg[word_idx] |= val << bit_idx;
		msg[word_idx+1] = val >> (32 - bit_idx);
		/* side effect: clear the new word when starting it */
	}
	return bit_off + 5*2;
}

void pd_dump_packet(void *ctxt, const char *msg)
{
	uint8_t *vals = ctxt;
	int bit;

	CPRINTF("ERR %s:\n000:- ", msg);
	/* Packet debug output */
	for (bit = 1; bit <  PD_MAX_RAW_SIZE; bit++) {
		int cnt = NB_PERIOD(vals[bit-1], vals[bit]);
		if ((bit & 31) == 0)
			CPRINTF("\n%03d:",bit);
		CPRINTF("%1d ",cnt);
	}
	CPRINTF("><\n");
	cflush();
	for (bit = 0; bit <  PD_MAX_RAW_SIZE; bit++) {
		if ((bit & 31) == 0)
			CPRINTF("\n%03d:",bit);
		CPRINTF("%02x ",vals[bit]);
	}
	CPRINTF("||\n");
	cflush();
}

/* --- SPI TX operation --- */

static const struct dma_option dma_tx_option = {
        STM32_DMAC_SPI1_TX, (void *)&SPI_REGS->dr,
        STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_16_BIT
};

void pd_start_tx(void *ctxt, int bit_len)
{
	stm32_dma_chan_t *tx = dma_get_channel(STM32_DMAC_SPI1_TX);

	/* disable RX detection interrupt */
	task_disable_irq(STM32_IRQ_COMP);
	/* set the low level reference */
	gpio_set_level(GPIO_PD_TX_EN, 0);
	/* put SPI function on TX pin */
	gpio_config_module(MODULE_USB_PD, 1);

	/* update DMA configuration */
	dma_prepare_tx(&dma_tx_option, DIV_ROUND_UP(bit_len, 8), ctxt);
	/* Flush data in write buffer so that DMA can get the lastest data */
	asm volatile("dmb;");

	/* Start counting at 300Khz*/
	STM32_TIM_CR1(TIM_TX) |= 1;
	/* Kick off the DMA to send the data */
	dma_go(tx);
}

static void enable_rx_monitoring(void)
{
	/* clear EXTI22 */
	STM32_EXTI_PR = 1 << 22;
	/* clean up older comparator event */
	task_clear_pending_irq(STM32_IRQ_COMP);
	/* re-enable comparator interrupt to detect packets */
	task_enable_irq(STM32_IRQ_COMP);
}

void pd_tx_done(void)
{
	stm32_spi_regs_t *spi = SPI_REGS;

	dma_wait(STM32_DMAC_SPI1_TX);
	/* wait for real end of transmission */
	while (!(spi->sr & (1<<1)))
		; /* wait for TXE == 1 */
	while (spi->sr & (1<<7))
		; /* wait for BSY == 0 */
	/* Stop counting */
	STM32_TIM_CR1(TIM_TX) &= ~1;
	/* clear tranfer flag */
	dma_clear_isr(STM32_DMAC_SPI1_TX);
	/* put SPI TX in Hi-Z */
	gpio_config_module(MODULE_USB_PD, 0);
	/* put the low level reference in Hi-Z */
	gpio_set_level(GPIO_PD_TX_EN, 1);

	/* re-enable the RX interrupt */
	enable_rx_monitoring();
}

/* --- RX operation using comparator linked to timer --- */

static const struct dma_option dma_tim_option = {
	STM32_DMAC_CH7, (void *)&STM32_TIM_CCR4(TIM_RX),
	STM32_DMA_CCR_MSIZE_8_BIT | STM32_DMA_CCR_PSIZE_16_BIT,
};

void pd_rx_complete(void)
{
	/* stop DMA */
	dma_disable(STM32_DMAC_CH7);
	/* stop stampling TIM2 */
	STM32_TIM_CR1(TIM_RX) &= 1;

	/* re-enable the RX interrupt */
	/*
         * do NOT re-enable RX yet, let's send quickly the GoodCRC first
         * enable_rx_monitoring();
         */
}

/* detect an edge on the PD RX pin */
void pd_rx_handler(void)
{
	if (1) /*pd_task_state == PD_STATE_IDLE)*/ {
		/* stop monitoring RX during sampling */
		task_disable_irq(STM32_IRQ_COMP);
		/* start sampling RX line on the ADC */
		dma_start_rx(&dma_tim_option, PD_MAX_RAW_SIZE, raw_samples);
		/* enable TIM2 DMA requests */
		STM32_TIM_SR(TIM_RX) = 0; /* clear overflows */
		STM32_TIM_CR1(TIM_RX) |= 1;
		/* trigger the analysis in the task */
		pd_rx_event();
	} else { /* Spurious watchdog ? */
		CPRINTF("!%04x/%04x\n",STM32_TIM_CCR4(TIM_RX),
			STM32_TIM_CNT(TIM_RX));
		/* one time ... */
		task_disable_irq(STM32_IRQ_COMP);
	}
	/* clear EXTI22 */
	STM32_EXTI_PR = 1 << 22;
}
DECLARE_IRQ(STM32_IRQ_COMP, pd_rx_handler, 1);

/* --- Startup initialization --- */
void *pd_hw_init(void)
{
	stm32_spi_regs_t *spi = SPI_REGS;

	/* --- SPI init --- */

	/* set 40 MHz pin speed */
	spi_set_pins_speed();

	/* Enable clocks to SPI module */
	spi_enable_clock();

	/* put SPI TX in Hi-Z */
	gpio_config_module(MODULE_USB_PD, 0);

	/* Enable Tx DMA for our first transaction */
	spi->cr2 = STM32_SPI_CR2_TXDMAEN;

	/* Enable the salve SPI: LSB first, force NSS, TX only */
	spi->cr1 = STM32_SPI_CR1_SPE | STM32_SPI_CR1_LSBFIRST
		 | STM32_SPI_CR1_SSM | STM32_SPI_CR1_BIDIMODE
		 | STM32_SPI_CR1_BIDIOE;

	/* configure TX DMA */
	dma_prepare_tx(&dma_tx_option, PD_MAX_RAW_SIZE, raw_samples);

	/* --- set TIM10 with updates at 300KHz (data baudrate) --- */
	__hw_timer_enable_clock(TIM_TX, 1);
	/* Timer configuration */
	STM32_TIM_CR1(TIM_TX) = 0x0000;
	STM32_TIM_CR2(TIM_TX) = 0x0000;
	STM32_TIM_DIER(TIM_TX) = 0x0000;
	/* Auto-reload value : 300000 Khz overflow */
	STM32_TIM_ARR(TIM_TX) = TX_CLOCK_DIV;
	/* 50% duty cycle on the output */
	STM32_TIM_CCR1(TIM_TX) = STM32_TIM_ARR(TIM_TX) / 2;
	/* Timer CH1 output configuration */
	STM32_TIM_CCMR1(TIM_TX) = (6 << 4) | (1 << 3);
	STM32_TIM_CCER(TIM_TX) = 1;
	/* set prescaler to /1 */
	STM32_TIM_PSC(TIM_TX) = 0;
	/* reset counter */
	STM32_TIM_CNT(TIM_TX) = 0;
	/* Reload the pre-scaler */
	STM32_TIM_EGR(TIM_TX) = 0x0001;
	/* Start counting */
	//STM32_TIM_CR1(TIM_TX) |= 1;
	/* 40 MHz pin speed on TIM10_CH1 (PB12) */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x03000000;

	/* --- set TIM2 for RX timing : 1.2Mhz rate, free-running --- */
	__hw_timer_enable_clock(TIM_RX, 1);
	/* Timer configuration */
	STM32_TIM_CR1(TIM_RX) = 0x0000;
	STM32_TIM_CR2(TIM_RX) = 0x0000;
	STM32_TIM_DIER(TIM_RX) = 0x0000;
	/* Auto-reload value : 16-bit free running counter */
	STM32_TIM_ARR(TIM_RX) = 0xFFFF;
	/* Timer IC4 input configuration */
	STM32_TIM_CCMR2(TIM_RX) = 1 << 8;
	STM32_TIM_CCER(TIM_RX) = (1<<15) | (1<<13) | (1<<12);
	/* configure DMA request on CCR4 update */
	STM32_TIM_DIER(TIM_RX) |= 1 << 12; /* CC4DE */;
	/* set prescaler to /26 (F=1.2Mhz, T=0.8us) */
	STM32_TIM_PSC(TIM_RX) = RX_CLOCK_DIV;
	/* reset counter */
	STM32_TIM_CNT(TIM_RX) = 0;
	/* Reload the pre-scaler */
	STM32_TIM_EGR(TIM_RX) = 0x0001 | (1<<4) /* clear CCR4 */;

	/* --- DAC configuration for comparator at 850mV --- */
	/* Enable DAC interface clock. */
	STM32_RCC_APB1ENR |= (1 << 29);
	/* set voltage Vout=0.850V (Vref = 3.0V) */
	STM32_DAC_DHR12RD = 850 * 4096 / 3000;
	/* Start DAC channel 1 */
	STM32_DAC_CR = STM32_DAC_CR_EN1;

	/* --- COMP2 as comparator for RX vs Vmid = 850mV --- */
#if defined(CHIP_FAMILY_STM32F0)
	/* 40 MHz pin speed on PA0 and PA4 */
	STM32_GPIO_OSPEEDR(GPIO_A) |= 0x303;
	/* turn on COMP/SYSCFG */
	STM32_RCC_APB2ENR |= 1 << 0;
	/* currently in hi-speed mode : TODO revisit later, INM = PA0 (aka INM6) */
	STM32_COMP_CSR = STM32_COMP_CMP1EN | STM32_COMP_CMP1MODE_HSPEED |
			 STM32_COMP_CMP1INSEL_INM6 |
			 STM32_COMP_CMP1OUTSEL_TIM1_IC1 |
			 STM32_COMP_CMP1HYST_HI;
#elif defined(CHIP_FAMILY_STM32L)
	/* 40 MHz pin speed on PB4 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0x300;

	STM32_RCC_APB1ENR |= 1 << 31; /* turn on COMP */

	STM32_COMP_CSR = STM32_COMP_OUTSEL_TIM2_IC4 | STM32_COMP_INSEL_DAC_OUT1
			| STM32_COMP_SPEED_FAST;
	/* route PB4 to COMP input2 through GR6_1 bit 4 (or PB5->GR6_2 / bit 5) */
	STM32_RI_ASCR2 |= 1 << 4;
#else
#error Unsupported chip family
#endif
	/* DBG */usleep(250000);
	/* comparator interrupt is controlled by EXTI22: triggers on falling edge */
	STM32_EXTI_FTSR |= 1 << 22;
	STM32_EXTI_IMR |= 1 << 22;
	task_enable_irq(STM32_IRQ_COMP);

	CPRINTF("USB PD initialized\n");
	return raw_samples;
}

static int command_txclock(int argc, char **argv)
{
	int freq;
	char *e;

        if (argc != 2)
                return EC_ERROR_PARAM_COUNT;

        freq = strtoi(argv[1], &e, 10);
        if (*e)
                return EC_ERROR_PARAM1;

	STM32_TIM_ARR(TIM_TX) = clock_get_freq() / (2*freq);
	ccprintf("set TX frequency to %d Hz (%d)\n",
		 freq, STM32_TIM_ARR(TIM_TX));

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(txclock, command_txclock,
                        "<freq>",
                        "set PD TX clock",
                        NULL);
