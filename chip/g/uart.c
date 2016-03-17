/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

struct uart_interrupts {
	int tx_int;
	int rx_int;
};
static struct uart_interrupts interrupt[] = {
	{GC_IRQNUM_UART0_TXINT, GC_IRQNUM_UART0_RXINT},
	{GC_IRQNUM_UART1_TXINT, GC_IRQNUM_UART1_RXINT},
	{GC_IRQNUM_UART2_TXINT, GC_IRQNUM_UART2_RXINT},
};
static int done_uart_init_yet;



#define USE_UART_INTERRUPTS (!(defined(CONFIG_CUSTOMIZED_RO) && \
			       defined(SECTION_IS_RO)))
int uart_init_done(void)
{
	return done_uart_init_yet;
}

void uart_tx_start(int uart)
{
	if (!uart_init_done())
		return;

	/* If interrupt is already enabled, nothing to do */
	if (GR_UART_ICTRL(uart) & GC_UART_ICTRL_TX_MASK)
		return;

	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);

	/*
	 * Re-enable the transmit interrupt, then forcibly trigger the
	 * interrupt.  This works around a hardware problem with the
	 * UART where the FIFO only triggers the interrupt when its
	 * threshold is _crossed_, not just met.
	 */
	/* TODO(crosbug.com/p/33819): Do we need this hack here? Find out. */
	REG_WRITE_MLV(GR_UART_ICTRL(uart), GC_UART_ICTRL_TX_MASK,
		      GC_UART_ICTRL_TX_LSB, 1);
	task_trigger_irq(interrupt[uart].tx_int);
}

void uart_tx_stop(int uart)
{
	/* Disable the TX interrupt */
	REG_WRITE_MLV(GR_UART_ICTRL(uart), GC_UART_ICTRL_TX_MASK,
		      GC_UART_ICTRL_TX_LSB, 0);

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

int uart_tx_in_progress(int uart)
{
	/* Transmit is in progress unless the TX FIFO is empty and idle. */
	return !(GR_UART_STATE(uart) & (GC_UART_STATE_TXIDLE_MASK |
				     GC_UART_STATE_TXEMPTY_MASK));
}

void uart_tx_flush(int uart)
{
	/* Wait until TX FIFO is idle. */
	while (uart_tx_in_progress(uart))
		;
}

int uart_tx_ready(int uart)
{
	/* True if the TX buffer is not completely full */
	return !(GR_UART_STATE(uart) & GC_UART_STATE_TX_MASK);
}

int uart_rx_available(int uart)
{
	/* True if the RX buffer is not completely empty. */
	return !(GR_UART_STATE(uart) & GC_UART_STATE_RXEMPTY_MASK);
}

void uart_write_char(int uart, char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready(uart))
		;

	GR_UART_WDATA(uart) = c;
}

int uart_read_char(int uart)
{
	return GR_UART_RDATA(uart);
}

#if USE_UART_INTERRUPTS
void uart_disable_interrupt(int uart)
{
	task_disable_irq(interrupt[uart].tx_int);
	task_disable_irq(interrupt[uart].rx_int);
}

void uart_enable_interrupt(int uart)
{
	task_enable_irq(interrupt[uart].tx_int);
	task_enable_irq(interrupt[uart].rx_int);
}

/**
 * Interrupt handlers for UART0
 */
void uart_ec_tx_interrupt(void)
{
	/* Clear transmit interrupt status */
	GR_UART_ISTATECLR(0) = GC_UART_ISTATECLR_TX_MASK;

	/* Fill output FIFO */
	uart_process_output();
}
DECLARE_IRQ(GC_IRQNUM_UART0_TXINT, uart_ec_tx_interrupt, 1);

void uart_ec_rx_interrupt(void)
{
	/* Clear receive interrupt status */
	GR_UART_ISTATECLR(0) = GC_UART_ISTATECLR_RX_MASK;

	/* Read input FIFO until empty */
	uart_process_input();
}
DECLARE_IRQ(GC_IRQNUM_UART0_RXINT, uart_ec_rx_interrupt, 1);
#endif  /* USE_UART_INTERRUPTS */

void uart_init(void)
{
	int uart = 1;
	int i;
	long long setting = (16 * (1 << UART_NCO_WIDTH) *
			     (long long)CONFIG_UART_BAUD_RATE / PCLK_FREQ);

	clock_enable_module(MODULE_UART, 1);

#ifdef UART_COUNT
	uart = UART_COUNT;
#endif
	/* turn on uart clock */
	for (i = 0; i < uart; i++) {
		/* set frequency */
		GR_UART_NCO(i) = setting;

		/*
		 * Interrupt when RX fifo has anything, when TX fifo <= half
		 * empty and reset (clear) both FIFOs
		 */
		GR_UART_FIFO(i) = 0x63;

		/*
		 * TX enable, RX enable, HW flow control disabled, no
		 * loopback
		 */
		GR_UART_CTRL(i) = 0x03;

		/* enable RX interrupts in block */
		/* Note: doesn't do anything unless turned on in NVIC */
		GR_UART_ICTRL(i) = 0x02;
	}

#if USE_UART_INTERRUPTS
	/* Enable interrupts for UART0 only */
	uart_enable_interrupt(0);
#endif

	done_uart_init_yet = 1;
}
