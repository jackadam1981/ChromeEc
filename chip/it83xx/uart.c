/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART module for Chrome EC */

#include "common.h"
#include "console.h"
#include "registers.h"
#include "task.h"
#include "uart.h"
#include "util.h"

static int init_done;

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_start(void)
{
	/* If interrupt is already enabled, nothing to do */
	if (IT83XX_UART_IER(0) & 0x03)
		return;

	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);

	/*
	 * Re-enable the transmit interrupt, then forcibly trigger the
	 * interrupt.  This works around a hardware problem with the
	 * UART where the FIFO only triggers the interrupt when its
	 * threshold is _crossed_, not just met.
	 */
	IT83XX_UART_IER(0) |= 0x03;
	task_trigger_irq(IT83XX_IRQ_UART1);
}

void uart_tx_stop(void)
{
	IT83XX_UART_IER(0) &= ~0x03;

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

void uart_tx_flush(void)
{
	/* Wait for transmit FIFO empty */
	while (!(IT83XX_UART_LSR(0) & 0x20))
		;
}

int uart_tx_ready(void)
{
	return IT83XX_UART_LSR(0) & 0x20;
}

int uart_tx_in_progress(void)
{
	return !(IT83XX_UART_LSR(0) & 0x40);
}

int uart_rx_available(void)
{
	return IT83XX_UART_LSR(0) & 0x01;
}

void uart_write_char(char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready())
		;

	IT83XX_UART_THR(0) = c;
}

int uart_read_char(void)
{
	return IT83XX_UART_RBR(0);
}

void uart_disable_interrupt(void)
{
	task_disable_irq(IT83XX_IRQ_UART1);
}

void uart_enable_interrupt(void)
{
	task_enable_irq(IT83XX_IRQ_UART1);
}

static void uart_ec_interrupt(void)
{
	/* clear interrupt status */
	IT83XX_INTC_ISR4 = 0x02;

	/* Read input FIFO until empty, then fill output FIFO */
	uart_process_input();
	uart_process_output();
}
DECLARE_IRQ(IT83XX_IRQ_UART1, uart_ec_interrupt, 1);

static void uart_config(int port)
{
	/* Set CLK_UART_DIV_SEL to /2. Assumes PLL is 48 MHz. */
	/* TODO: depends on clock source */
	IT83XX_ECPM_SCDCR1 |= 0x01;

	/* Specify clock source of the UART is 24MHz, must match CLK_UART_DIV_SEL. */
	/* TODO: depends on clock source */
	IT83XX_UART_CSSR(port) = 0x1;

	/* 8-N-1 and DLAB set to allow access to DLL and DLM registers. */
	IT83XX_UART_LCR(port) = 0x83;

	/* Set divisor to set baud rate to 115200 */
	IT83XX_UART_DLM(port) = 0x00;
	IT83XX_UART_DLL(port) = 0x01;

	/*
	 * Clear DLAB bit to exclude access to DLL and DLM and give access to
	 * RBR and THR.
	 */
	IT83XX_UART_LCR(port) = 0x03;

	/*
	 * Enable TX and RX FIFOs and set RX FIFO interrupt level to the
	 * minimum 1 byte.
	 */
	IT83XX_UART_FCR(port) = 0x07;
}

void uart_init(void)
{
	/* Enable clocks to UART 1 and 2. */
	IT83XX_ECPM_CGCTRL3R |= 0x44;

	/* Config UART 0 only for now. */
	uart_config(0);

	/* clear interrupt status */
	IT83XX_INTC_ISR4 = 0x02;

	/* Enable interrupts */
	IT83XX_UART_IER(0) = 0x03;
	task_enable_irq(IT83XX_IRQ_UART1);

	init_done = 1;
}
