/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART module for Chrome EC */

#include "clock.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

/* Traces on UARTA */
#define UART_PORT A

static int init_done;

int uart_init_done(void)
{
	return init_done;
}

void uart_tx_start(void)
{
	/* If interrupt is already enabled, nothing to do */
	if (AVP_UART_IER(UART_PORT) & 0x02)
		return;

	/* Do not allow deep sleep while transmit in progress */
	disable_sleep(SLEEP_MASK_UART);

	/* Re-enable the transmit interrupt. */
	AVP_UART_IER(UART_PORT) |= 0x02;
}

void uart_tx_stop(void)
{
	AVP_UART_IER(UART_PORT) &= ~0x02;

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

void uart_tx_flush(void)
{
	/*
	 * Wait for transmit FIFO empty (TEMT) and transmitter holder
	 * register and transmitter shift registers to be empty (THRE).
	 */
	while ((AVP_UART_LSR(UART_PORT) & 0x60) != 0x60)
		;
}

int uart_tx_ready(void)
{
	/* Transmit is ready when FIFO is empty (THRE). */
	return AVP_UART_LSR(UART_PORT) & 0x20;
}

int uart_tx_in_progress(void)
{
	/*
	 * Transmit is in progress if transmit holding register or transmitter
	 * shift register are not empty (TEMT).
	 */
	return !(AVP_UART_LSR(UART_PORT) & 0x40);
}

int uart_rx_available(void)
{
	return AVP_UART_LSR(UART_PORT) & 0x01;
}

void uart_write_char(char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready())
		;

	AVP_UART_THR(UART_PORT) = c;
}

int uart_read_char(void)
{
	return AVP_UART_RBR(UART_PORT);
}

void uart_disable_interrupt(void)
{
	task_disable_irq(AVP_IRQ_UARTA);
}

void uart_enable_interrupt(void)
{
	task_enable_irq(AVP_IRQ_UARTA);
}

static void uart_ec_interrupt(void)
{
	/* clear interrupt status */
	task_clear_pending_irq(AVP_IRQ_UARTA);

	/* Read input FIFO until empty, then fill output FIFO */
	uart_process_input();
	uart_process_output();
}
DECLARE_IRQ(AVP_IRQ_UARTA, uart_ec_interrupt, 1);

static void uart_config(void)
{
	/* 8-N-1 and DLAB set to allow access to DLL and DLM registers. */
	AVP_UART_LCR(UART_PORT) = 0x83;

	/* Set divisor to set baud rate to 115200 */
	AVP_UART_DLM(UART_PORT) = 0x00;
	AVP_UART_DLL(UART_PORT) = 221;

	/*
	 * Clear DLAB bit to exclude access to DLL and DLM and give access to
	 * RBR and THR.
	 */
	AVP_UART_LCR(UART_PORT) = 0x03;

	/*
	 * Enable TX and RX FIFOs and set RX FIFO interrupt level to the
	 * minimum 1 byte.
	 */
	AVP_UART_FCR(UART_PORT) = 0x07;

	/* Disable hw flow control and force DTR and RTS */
	AVP_UART_MCR(UART_PORT) = 0x03;
}

void uart_init(void)
{
	/* Waiting for when we can use the GPIO module to set pin muxing */
	gpio_config_module(MODULE_UART, 1);

	/* Enable clocks to UARTA. */
	clock_enable_peripheral(CAR_OFFSET_UARTA, 0, 0);

	/* Config UART A only for now. */
	uart_config();

	/* clear interrupt status */
	task_clear_pending_irq(AVP_IRQ_UARTA);

	/* Enable interrupts */
	AVP_UART_IER(UART_PORT) = 0x03;
	task_enable_irq(AVP_IRQ_UARTA);

	init_done = 1;
}
