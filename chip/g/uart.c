/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "registers.h"
#include "uart.h"

#define UART_NCO ((16 * (1 << UART_NCO_WIDTH) *				\
		   (long long)CONFIG_UART_BAUD_RATE) / PCLK_FREQ)

#if defined CONFIG_POLLING_UART

/* 115200N81 uart0, TX on A0, RX on A1 */
void uart_init(void)
{
	/* Pinmux init also turns on all clocks. */
	GREG32(PMU, PERICLKSET0) = 0xffffffff;
	GREG32(PMU, PERICLKSET1) = 0xffffffff;

	/*
	 * hardwire clocks to some value... just to get going
	 * Set source of trim to calibration logic during dynamic trim
	 */
	GWRITE_FIELD(XO, CLK_TIMER_TRIM_CTRL, RC_COARSE_TRIM_SRC, 0);

	/* Set initial coarse trim value (slowest) */
	GREG32(XO, CLK_TIMER_RC_COARSE_ATE_TRIM) = 100;

	/* Set initial trim stabilization period */
	GWRITE_FIELD(XO, CLK_TIMER_TRIM_CTRL, RC_INITIAL_TRIM_PERIOD, 10);

	/* enable trim */
	GWRITE_FIELD(XO, CLK_TIMER_TRIM_CTRL, RC_TRIM_EN, 1);

	/* domain crossing sync */
	GREG32(XO, CLK_TIMER_SYNC_CONTENTS) =  0x1;


	GREG32(PINMUX, DIOA0_SEL) = GC_PINMUX_UART0_TX_SEL;
	GREG32(PINMUX, UART0_RX_SEL) = GC_PINMUX_DIOA1_SEL;
	GREG32(PINMUX, DIOA1_CTL) =
		GC_PINMUX_DIOA1_CTL_DS_MASK  | GC_PINMUX_DIOA1_CTL_IE_MASK;

	GREG32(PMU, PWRDN_SCRATCH3) = 0xbeefcafe;

	GREG32(UART, FIFO) = 3;  /* clear RX,TX FIFO */

	GREG32(UART, NCO) = UART_NCO;  /* 115200N81 */

	GREG32(UART, CTRL) = 3;  /* TX,RX enable */
	uart_write_char('\n');
	uart_write_char('\r');
}

int uart_tx_ready(void)
{
	/*
	 * This makes sure that transmit FIFO is fully flashed, so that TX
	 * FIFO is not used.
	 */
	return GREAD_FIELD(UART, STATE, TXIDLE);
}

void uart_tx_flush(void)
{
}

int uart_init_done(void)
{
	return 1;
}
void uart_tx_start(void)
{
}
void uart_tx_stop(void)
{
}

void uart_write_char(char c)
{
	while (!uart_tx_ready())
		;
	GREG32(UART, WDATA) = c;
}

#else  /* CONFIG_POLLING_UART   ^^^^^ YES    vvvvvv NO */

#include "clock.h"
#include "gpio.h"
#include "system.h"
#include "task.h"
#include "util.h"

static int done_uart_init_yet;

int uart_init_done(void)
{
	return done_uart_init_yet;
}

void uart_tx_start(void)
{
	if (!uart_init_done())
		return;

	/* If interrupt is already enabled, nothing to do */
	if (GR_UART_ICTRL(0) & GC_UART_ICTRL_TX_MASK)
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
	REG_WRITE_MLV(GR_UART_ICTRL(0), GC_UART_ICTRL_TX_MASK,
		      GC_UART_ICTRL_TX_LSB, 1);
	task_trigger_irq(GC_IRQNUM_UART0_TXINT);
}

void uart_tx_stop(void)
{
	/* Disable the TX interrupt */
	REG_WRITE_MLV(GR_UART_ICTRL(0), GC_UART_ICTRL_TX_MASK,
		      GC_UART_ICTRL_TX_LSB, 0);

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

int uart_tx_in_progress(void)
{
	/* Transmit is in progress if the TX idle bit is not set */
	return !(GR_UART_STATE(0) & GC_UART_STATE_TXIDLE_MASK);
}

void uart_tx_flush(void)
{
	/* Wait until TX FIFO is idle. */
	while (uart_tx_in_progress())
		;
}

int uart_tx_ready(void)
{
	/* True if the TX buffer is not completely full */
	return !(GR_UART_STATE(0) & GC_UART_STATE_TX_MASK);
}

int uart_rx_available(void)
{
	/* True if the RX buffer is not completely empty. */
	return !(GR_UART_STATE(0) & GC_UART_STATE_RXEMPTY_MASK);
}

void uart_write_char(char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uart_tx_ready())
		;

	GR_UART_WDATA(0) = c;
}

int uart_read_char(void)
{
	return GR_UART_RDATA(0);
}

void uart_disable_interrupt(void)
{
	task_disable_irq(GC_IRQNUM_UART0_TXINT);
	task_disable_irq(GC_IRQNUM_UART0_RXINT);
}

void uart_enable_interrupt(void)
{
	task_enable_irq(GC_IRQNUM_UART0_TXINT);
	task_enable_irq(GC_IRQNUM_UART0_RXINT);
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

void uart_init(void)
{
	/* turn on uart clock */
	clock_enable_module(MODULE_UART, 1);

	/* set frequency */
	GR_UART_NCO(0) = UART_NCO;

	/* Interrupt when RX fifo has anything, when TX fifo <= half empty */
	/* Also reset (clear) both FIFOs */
	GR_UART_FIFO(0) = 0x63;

	/* TX enable, RX enable, HW flow control disabled, no loopback */
	GR_UART_CTRL(0) = 0x03;

	/* enable RX interrupts in block */
	/* Note: doesn't do anything unless turned on in NVIC */
	GR_UART_ICTRL(0) = 0x02;

	/* Enable interrupts for UART0 only */
	uart_enable_interrupt();

	done_uart_init_yet = 1;
}
#endif  /* CONFIG_POLLING_UART   ^^^^^  NO */
