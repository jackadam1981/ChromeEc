/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hwtimer.h"
#include "lpc.h"
#include "registers.h"
#include "clock_chip.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

static int init_done;

#ifdef CONFIG_UART_PAD_SWITCH
/* Current pad: 0 for default pad, 1 for alternate. */
static volatile enum uart_pad pad;

/*
 * When switched to alternate pad, read/write data according to the parameters
 * below.
 */
static volatile uint8_t *altpad_rx_buf;
static volatile int altpad_rx_pos;
static volatile int altpad_rx_len;
static volatile uint8_t *altpad_tx_buf;
static volatile int altpad_tx_pos;
static volatile int altpad_tx_len;

/*
 * Time we last received a byte on default UART, we do not allow use of
 * alternate pad for block_alt_timeout_us after that, to make sure input
 * characters are not lost (eitehr interactively, or though servod/FAFT).
 */
static timestamp_t last_default_pad_rx_time;

static const uint32_t block_alt_timeout_us = 500*MSEC;
#endif

static inline enum uart_pad get_current_pad(void)
{
#ifdef CONFIG_UART_PAD_SWITCH
	return pad;
#else
	return UART_DEFAULT_PAD;
#endif
}

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_start(void)
{
	/* We needn't to switch uart from gpio again in npcx7. */
#if defined(CHIP_FAMILY_NPCX5)
	if (uart_is_enable_wakeup() && get_current_pad() == UART_DEFAULT_PAD) {
		/* disable MIWU */
		uart_enable_wakeup(0);
		/* Set pin-mask for UART */
		npcx_gpio2uart(0);
		/* enable uart again from MIWU mode */
		task_enable_irq(NPCX_IRQ_UART);
	}
#endif

	/* If interrupt is already enabled, nothing to do */
	if (NPCX_UICTRL & 0x20)
		return;

	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);

	/*
	 * Re-enable the transmit interrupt, then forcibly trigger the
	 * interrupt.  This works around a hardware problem with the
	 * UART where the FIFO only triggers the interrupt when its
	 * threshold is _crossed_, not just met.
	 */
	NPCX_UICTRL |= 0x20;

	task_trigger_irq(NPCX_IRQ_UART);
}

void uart_tx_stop(void)	/* Disable TX interrupt */
{
	NPCX_UICTRL &= ~0x20;

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

void uart_tx_flush(void)
{
	/* Wait for transmit FIFO empty */
	while (!(NPCX_UICTRL & 0x01))
		;
	/* Wait for transmitting completed */
	while (NPCX_USTAT & 0x40)
		;
}

int uart_tx_ready(void)
{
	return NPCX_UICTRL & 0x01;	/*if TX FIFO is empty return 1*/
}

int uart_tx_in_progress(void)
{
	/* Transmit is in progress if the TX busy bit is set. */
	return NPCX_USTAT & 0x40;	/*BUSY bit , if busy return 1*/
}

int uart_rx_available(void)
{
	uint8_t ctrl = NPCX_UICTRL;
	if (ctrl & 0x02) {
#ifdef CONFIG_LOW_POWER_IDLE
		/*
		 * Activity seen on UART RX pin while UART was disabled for deep
		 * sleep. The console won't see that character because the UART
		 * is disabled, so we need to inform the clock module of UART
		 * activity ourselves.
		 */
		clock_refresh_console_in_use();
#endif
#ifdef CONFIG_UART_PAD_SWITCH
		if (get_current_pad() == UART_DEFAULT_PAD)
			last_default_pad_rx_time = get_time();
#endif
	}
	return ctrl & 0x02; /* If RX FIFO is empty return '0'*/
}

void uart_write_char(char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready())
		;

	NPCX_UTBUF = c;
}

int uart_read_char(void)
{
	return NPCX_URBUF;
}

void uart_clear_rx_fifo(int channel)
{
	int scratch __attribute__ ((unused));
	if (channel == 0) { /* suppose '0' is EC UART*/
		/*if '1' that mean have a RX data on the FIFO register*/
		while ((NPCX_UICTRL & 0x02))
			scratch = NPCX_URBUF;
	}
}

/**
 * Interrupt handler for UART0
 */
void uart_ec_interrupt(void)
{
#ifdef CONFIG_UART_PAD_SWITCH
	if (pad == UART_ALTERNATE_PAD) {
		if (uart_rx_available()) {
			uint8_t c = uart_read_char();

			if (altpad_rx_pos < altpad_rx_len)
				altpad_rx_buf[altpad_rx_pos++] = c;
		}
		if (uart_tx_ready()) {
			if (altpad_tx_pos < altpad_tx_len)
				uart_write_char(altpad_tx_buf[altpad_tx_pos++]);
			else
				uart_tx_stop();
		}
		return;
	}
#endif

	/* Default pad. */
	/* Read input FIFO until empty, then fill output FIFO */
	uart_process_input();
	uart_process_output();
}
DECLARE_IRQ(NPCX_IRQ_UART, uart_ec_interrupt, 0);

#ifdef CONFIG_UART_PAD_SWITCH
void uart_set_pad(enum uart_pad newpad)
{
	task_disable_irq(NPCX_IRQ_UART);

	/* Flush the last byte */
	uart_tx_flush();
	uart_tx_stop();

	/*
	 * Shut down UART, this seems to be required, otherwise the controller
	 * may sometimes get stuck in a state where no data can be received
	 * anymore on either pad.
	 */
	clock_disable_peripheral(CGC_OFFSET_UART, 0x10, CGC_MODE_ALL);

	pad = newpad;

	npcx_gpio2uart(pad);

	clock_enable_peripheral(CGC_OFFSET_UART, 0x10, CGC_MODE_ALL);

	uart_clear_rx_fifo(0);

	task_enable_irq(NPCX_IRQ_UART);
}

void uart_default_pad_rx_interrupt(enum gpio_signal signal)
{
	/*
	 * We received an interrupt on the primary pad, give up on the
	 * transaction and switch back.
	 */
	gpio_disable_interrupt(GPIO_UART_MAIN_RX);
	uart_set_pad(UART_DEFAULT_PAD);
	last_default_pad_rx_time = get_time();
}

int uart_alt_pad_read_write(uint8_t *tx, int tx_len, uint8_t *rx, int rx_len,
			    int timeout_us)
{
	uint32_t start = __hw_clock_source_read();
	uint32_t delta;
	int ret = 0;

	if ((get_time().val - last_default_pad_rx_time.val)
			< block_alt_timeout_us)
		return -EC_ERROR_BUSY;

	cflush();

	altpad_rx_buf = rx;
	altpad_rx_pos = 0;
	altpad_rx_len = rx_len;
	altpad_tx_buf = tx;
	altpad_tx_pos = 0;
	altpad_tx_len = tx_len;

	uart_set_pad(UART_ALTERNATE_PAD);
	gpio_clear_pending_interrupt(GPIO_UART_MAIN_RX);
	gpio_enable_interrupt(GPIO_UART_MAIN_RX);
	uart_tx_start();

	do {
		/* Pad switched during transaction. */
		if (pad != UART_ALTERNATE_PAD) {
			ret = -EC_ERROR_BUSY;
			goto out;
		}

		if (altpad_rx_pos == altpad_rx_len &&
		    altpad_tx_pos == altpad_tx_len)
			break;

		delta = __hw_clock_source_read() - start;

		usleep(100);
	} while (delta < timeout_us);

	gpio_disable_interrupt(GPIO_UART_MAIN_RX);
	uart_set_pad(UART_DEFAULT_PAD);

	if (altpad_tx_pos == altpad_tx_len)
		ret = altpad_rx_pos;
	else
		ret = -EC_ERROR_TIMEOUT;

out:
	altpad_rx_len = 0;
	altpad_rx_pos = 0;
	altpad_rx_buf = NULL;
	altpad_tx_len = 0;
	altpad_tx_pos = 0;
	altpad_tx_buf = NULL;

	return ret;
}
#endif

static void uart_config(void)
{
	/* Configure pins from GPIOs to CR_UART */
	gpio_config_module(MODULE_UART, 1);

	/* Enable MIWU IRQ of UART */
#if NPCX_UART_MODULE2
	task_enable_irq(NPCX_IRQ_WKINTG_1);
#else
	task_enable_irq(NPCX_IRQ_WKINTB_1);
#endif
	/*
	 * Configure the UART wake-up event triggered from a falling edge
	 * on CR_SIN pin.
	 */
#if defined(CHIP_FAMILY_NPCX7) && defined(CONFIG_LOW_POWER_IDLE)
	SET_BIT(NPCX_WKEDG(MIWU_TABLE_1, MIWU_GROUP_8), 7);
#endif
	/*
	 * If apb2's clock is not 15MHz, we need to find the other optimized
	 * values of UPSR and UBAUD for baud rate 115200.
	 */
#if (NPCX_APB_CLOCK(2) != 15000000)
#error "Unsupported apb2 clock for UART!"
#endif

	/* Fix baud rate to 115200 */
	NPCX_UPSR = 0x38;
	NPCX_UBAUD = 0x01;

	/*
	 * 8-N-1, FIFO enabled.  Must be done after setting
	 * the divisor for the new divisor to take effect.
	 */
	NPCX_UFRS = 0x00;
	NPCX_UICTRL = 0x40; /* receive int enable only */
}

void uart_init(void)
{
	uint32_t mask = 0;

	/*
	 * Enable UART0 in run, sleep, and deep sleep modes. Enable the Host
	 * UART in run and sleep modes.
	 */
	mask = 0x10; /* bit 4 */
	clock_enable_peripheral(CGC_OFFSET_UART, mask, CGC_MODE_ALL);

	/* Set pin-mask for UART */
	npcx_gpio2uart(0);

	/* Configure UARTs (identically) */
	uart_config();

	/*
	 * Enable interrupts for UART0 only. Host UART will have to wait
	 * until the LPC bus is initialized.
	 */
	uart_clear_rx_fifo(0);
	task_enable_irq(NPCX_IRQ_UART);

	init_done = 1;
}
