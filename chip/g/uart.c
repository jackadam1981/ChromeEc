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
#include "uartn.h"
#include "util.h"

static int done_uart_init_yet;

#ifndef UART_COUNT
#define UART_COUNT 1
#endif

#define USE_UART_INTERRUPTS (!(defined(CONFIG_CUSTOMIZED_RO) && \
			       defined(SECTION_IS_RO)))
int uart_init_done(void)
{
	return done_uart_init_yet;
}

void uart_tx_start(void)
{
	uartn_tx_start(0);
}

void uart_tx_stop(void)
{
	uartn_tx_stop(0);

}

int uart_tx_in_progress(void)
{
	return uartn_tx_in_progress(0);
}

void uart_tx_flush(void)
{
	uartn_tx_flush(0);
}

int uart_tx_ready(void)
{
	/* True if the TX buffer is not completely full */
	return uartn_tx_ready(0);
}

int uart_rx_available(void)
{
	/* True if the RX buffer is not completely empty. */
	return uartn_rx_available(0);
}

void uart_write_char(char c)
{
	uartn_write_char(0, c);
}

int uart_read_char(void)
{
	return uartn_read_char(0);
}

#if USE_UART_INTERRUPTS
void uart_disable_interrupt(void)
{
	uartn_disable_interrupt(0);
}

void uart_enable_interrupt(void)
{
	uartn_enable_interrupt(0);
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
	int i;

	clock_enable_module(MODULE_UART, 1);

	/* Initialize all UARTs */
	for (i = 0; i < UART_COUNT; i++)
		uartn_init(i);

#if USE_UART_INTERRUPTS
	/* Enable interrupts for UART0 only */
	uart_enable_interrupt();
#endif

	done_uart_init_yet = 1;
}
