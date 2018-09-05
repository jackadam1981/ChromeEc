/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SCP UART module */

#include "console.h"
#include "registers.h"
#include "serial_reg.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "util.h"

/* Console UART index */
#define UARTN           CONFIG_UART_CONSOLE

static const unsigned int uart_wait_us = 50;
static const unsigned int uart_idle_wait_us = 500;
static uint8_t uart_done, tx_started;

int uart_init_done(void)
{
	/*
	 * TODO: AP UART support
	 * When access AP UART port, wait for AP peripheral clock
	 */
	return uart_done;
}

void uart_tx_start(void)
{
	tx_started = 1;

	/* AP UART mode doesn't support interrupt */
	if (UARTN >= SCP_UART_COUNT)
		return;

	if (UART_IIR(UARTN) & UART_IIR_THRI)
		return;

	disable_sleep(SLEEP_MASK_UART);
	/*
	 * IER is a write only register, fill other en bits when
	 * enable xmit interrupt.
	 */
	UART_IER(UARTN) = UART_IER_RDI | UART_IER_THRI;
}

void uart_tx_stop(void)
{
	tx_started = 0;

	/* AP UART mode doesn't support interrupt */
	if (UARTN >= SCP_UART_COUNT)
		return;

	UART_IER(UARTN) = UART_IER_RDI;
	enable_sleep(SLEEP_MASK_UART);
}

void uart_tx_flush(void)
{
	while (!(UART_LSR(UARTN) & UART_LSR_TEMT))
		usleep(uart_wait_us);
}

int uart_tx_ready(void)
{
	/* Check xmit FIFO empty */
	return (UART_LSR(UARTN) & UART_LSR_THRE);
}

int uart_rx_available(void)
{
	/* Check rcvr data ready */
	return (UART_LSR(UARTN) & UART_LSR_DR);
}

void uart_write_char(char c)
{
	while (!uart_tx_ready())
		usleep(uart_wait_us);

	UART_DATA(UARTN) = c;
}

int uart_read_char(void)
{
	return UART_DATA(UARTN);
}

static void uart_process(void)
{
	uart_process_input();
	uart_process_output();
}

#if (UARTN < SCP_UART_COUNT)
void uart_interrupt(void)
{
	task_clear_irq(UART_IRQ(UARTN));
	uart_process();
}
DECLARE_IRQ(UART_IRQ(UARTN), uart_interrupt, 2);

void uart_interrupt2(void)
{
	task_clear_irq(UART_RX_IRQ(UARTN));
	uart_process();
}
DECLARE_IRQ(UART_RX_IRQ(UARTN), uart_interrupt, 2);
#endif

void uart_task(void)
{
	while (1) {
#if (UARTN < SCP_UART_COUNT)
		task_wait_event(0);
#else
		if (uart_rx_available() || tx_started)
			uart_process();
		else
			task_wait_event(uart_idle_wait_us);
	}
#endif
}

#ifdef CONFIG_LOW_POWER_IDLE
void uart_enter_dsleep(void)
{
}

void uart_exit_dsleep(void)
{
}

void uart_deepsleep_interrupt(enum gpio_signal signal)
{
}
#endif

void uart_init(void)
{
	const uint32_t baud_rate = CONFIG_UART_BAUD_RATE;
	const uint32_t uart_clock = 26000000;
	const uint32_t div = (uart_clock + (baud_rate * 8)) / (baud_rate * 16);
	/* Init and clear FIFO */
	UART_FCR(UARTN) = UART_FCR_ENABLE_FIFO
		| UART_FCR_CLEAR_RCVR
		| UART_FCR_CLEAR_XMIT
		| UART_FCR_T_TRIG_01    /* TX threshold 4 bytes */
		| UART_FCR_R_TRIG_10;   /* RX threshold 12 bytes */
	/* Line control: parity none, 8 bit, 1 stop bit */
	UART_LCR(UARTN) = UART_LCR_WLEN8;
	/* For baud rate <= 115200 */
	UART_HIGHSPEED(UARTN) = 0;
	/* DLAB = 1 and update DLL DLH */
	UART_LCR(UARTN) |= UART_LCR_DLAB;
	UART_DLL(UARTN) = div & 0xff;
	UART_DLH(UARTN) = (div >> 8) & 0xff;
	UART_LCR(UARTN) &= ~UART_LCR_DLAB;
#if (UARTN < SCP_UART_COUNT)
	task_enable_irq(UART_IRQ(UARTN));
	task_enable_irq(UART_RX_IRQ(UARTN));
#endif
	uart_done = 1;
}

