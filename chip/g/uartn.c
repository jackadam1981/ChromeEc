/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "uart.h"
#include "uart_bitbang.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

#define USE_UART_INTERRUPTS (!(defined(CONFIG_CUSTOMIZED_RO) && \
			       defined(SECTION_IS_RO)))

#define UART_COUNT     3

struct uartn_interrupts {
	int tx_int;
	int rx_int;
};
static struct uartn_interrupts interrupt[] = {
	{GC_IRQNUM_UART0_TXINT, GC_IRQNUM_UART0_RXINT},
	{GC_IRQNUM_UART1_TXINT, GC_IRQNUM_UART1_RXINT},
	{GC_IRQNUM_UART2_TXINT, GC_IRQNUM_UART2_RXINT},
};

struct uartn_function_ptrs uartn_funcs[3] = {
	{
		_uartn_rx_available,
		_uartn_write_char,
		_uartn_read_char,
	},

	{
		_uartn_rx_available,
		_uartn_write_char,
		_uartn_read_char,
	},

	{
		_uartn_rx_available,
		_uartn_write_char,
		_uartn_read_char,
	},
};

void uartn_tx_start(int uart)
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

void uartn_tx_stop(int uart)
{
	/* Disable the TX interrupt */
	REG_WRITE_MLV(GR_UART_ICTRL(uart), GC_UART_ICTRL_TX_MASK,
		      GC_UART_ICTRL_TX_LSB, 0);

	/* Re-allow deep sleep */
	enable_sleep(SLEEP_MASK_UART);
}

int uartn_tx_in_progress(int uart)
{
	/* Transmit is in progress unless the TX FIFO is empty and idle. */
	return !(GR_UART_STATE(uart) & (GC_UART_STATE_TXIDLE_MASK |
				     GC_UART_STATE_TXEMPTY_MASK));
}

void uartn_tx_flush(int uart)
{
	timestamp_t ts;
	int i;

	/* Wait until TX FIFO is idle. */
	while (uartn_tx_in_progress(uart))
		;
	/*
	 * Even when uartn_tx_in_progress() returns false, the chip seems to
	 * be still trasmitting, resetting at this point results in an eaten
	 * last symbol. Let's just wait some time (required to transmit 10
	 * bits at 115200 baud).
	 */
	ts = get_time(); /* Start time. */
	for (i = 0; i < 1000; i++) /* Limit it in case timer is not running. */
		if ((get_time().val - ts.val) > ((1000000 * 10) / 115200))
			return;
}

int uartn_tx_ready(int uart)
{
	/* True if the TX buffer is not completely full */
	return !(GR_UART_STATE(uart) & GC_UART_STATE_TX_MASK);
}

int _uartn_rx_available(int uart)
{
	/* True if the RX buffer is not completely empty. */
	return !(GR_UART_STATE(uart) & GC_UART_STATE_RXEMPTY_MASK);
}

int uartn_rx_available(int uart)
{
	return uartn_funcs[uart]._rx_available(uart);
}

void _uartn_write_char(int uart, char c)
{
	/* Wait for space in transmit FIFO. */
	while (!uartn_tx_ready(uart))
		;

	GR_UART_WDATA(uart) = c;
}

void uartn_write_char(int uart, char c)
{
	uartn_funcs[uart]._write_char(uart, c);
}

#if USE_UART_INTERRUPTS

/*
 * Counter of break characters received in a row, per UART channel. Receiving
 * 3 or more is considered an indication of the UART RX line held low by the
 * inactive TX.
 *
 * Once storm condition is identified, the same value is sued to control UART
 * RX state transitions:
 *
 *   4 - interrupt disable pending
 * 100 - interrupt disabled
 */
enum {
	IRQS_MAX_BREAKS_IN_A_ROW = 3,
	IRQS_INTERRUPT_DISABLE_PENDING = 4,
	IRQS_INTERRUPT_DISABLED = 100,
};
static uint8_t breaks_in_a_row[UART_COUNT];

/* Flag to tell if the reenable UART hook is running. */
static uint8_t reenable_hook_is_running;

/* G chip has large FIFOs. */
#define G_RX_FIFO_SIZE 32

const struct deferred_data reenable_rx_irq_when_ready_data;
static void reenable_rx_irq_when_ready(void)
{
	size_t i;
	int keep_going = 0;

	for (i = 0; i < ARRAY_SIZE(breaks_in_a_row); i++) {
		int loop_limit;
		int got_nonzero_data;

		if (breaks_in_a_row[i] != IRQS_INTERRUPT_DISABLED)
			continue;

		/*
		 * For all channels which where interrupt storm was detected
		 * check if storm is still on. This would be indicated by FIFO
		 * filled up with 32 zeros. If FIFO is less than 32 characters
		 * full, or there is a non-zero character, the storm is over.
		 */
		keep_going++;
		loop_limit = 0;
		got_nonzero_data = 0;
		while (_uartn_rx_available(i) &&
		       (loop_limit++ < G_RX_FIFO_SIZE))
			if (GR_UART_RDATA(i)) {
				got_nonzero_data = 1;
				break;
			}

		if ((loop_limit < G_RX_FIFO_SIZE) || got_nonzero_data) {
			breaks_in_a_row[i] = 0;
			task_enable_irq(interrupt[i].rx_int);
			CPRINTS("Re-enabled UART%d (limit at %d)",
				i, loop_limit);
			keep_going--;
		}
	}

	if (!keep_going) {
		reenable_hook_is_running = 0;
		return;
	}

	hook_call_deferred(&reenable_rx_irq_when_ready_data, 1 * SECOND);
}
DECLARE_DEFERRED(reenable_rx_irq_when_ready);

static void disable_rx_irq(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(breaks_in_a_row); i++)
		if (breaks_in_a_row[i] == IRQS_INTERRUPT_DISABLE_PENDING) {
			task_disable_irq(interrupt[i].rx_int);
			CPRINTS("Disabled UART%d due to inerrupt storm", i);
			breaks_in_a_row[i] = IRQS_INTERRUPT_DISABLED;
			if (!reenable_hook_is_running)
				reenable_hook_is_running = 1;
			hook_call_deferred(&reenable_rx_irq_when_ready_data,
					   1 * SECOND);
		}
}
DECLARE_DEFERRED(disable_rx_irq);

#endif

int _uartn_read_char(int uart)
{
	int c;

	c = GR_UART_RDATA(uart);

#if USE_UART_INTERRUPTS
	if (c) {
		breaks_in_a_row[uart] = 0;
	} else {
		uint8_t breaks = breaks_in_a_row[uart];

		if (breaks < IRQS_MAX_BREAKS_IN_A_ROW) {
			breaks_in_a_row[uart] = breaks + 1;
		} else if (breaks == IRQS_MAX_BREAKS_IN_A_ROW) {
			breaks_in_a_row[uart] = IRQS_INTERRUPT_DISABLE_PENDING;
			hook_call_deferred(&disable_rx_irq_data, 0);
		}
	}
#endif
	return c;
}

int uartn_read_char(int uart)
{
	return uartn_funcs[uart]._read_char(uart);
}

#ifdef CONFIG_UART_BITBANG
int _uart_bitbang_rx_available(int uart)
{
	if (uart_bitbang_is_enabled(uart))
		return uart_bitbang_is_char_available(uart);

	return 0;
}

void _uart_bitbang_write_char(int uart, char c)
{
	if (uart_bitbang_is_enabled(uart))
		uart_bitbang_write_char(uart, c);
}

int _uart_bitbang_read_char(int uart)
{
	if (uart_bitbang_is_enabled(uart))
		return uart_bitbang_read_char(uart);

	return 0;
}
#endif /* defined(CONFIG_UART_BITBANG) */

void uartn_disable_interrupt(int uart)
{
	task_disable_irq(interrupt[uart].tx_int);
	task_disable_irq(interrupt[uart].rx_int);
}

void uartn_enable_interrupt(int uart)
{
	task_enable_irq(interrupt[uart].tx_int);
	task_enable_irq(interrupt[uart].rx_int);
}


void uartn_enable(int uart)
{
	/* Enable TX and RX. Disable HW flow control and loopback. */
	GR_UART_CTRL(uart) = 0x03;
}

/* Disable TX, RX, HW flow control, and loopback */
void uartn_disable(int uart)
{
	GR_UART_CTRL(uart) = 0;
}

int uartn_is_enabled(int uart)
{
	return !!(GR_UART_CTRL(uart) & 0x03);
}

void uartn_init(int uart)
{
	long long setting = (16 * (1 << UART_NCO_WIDTH) *
			     (long long)CONFIG_UART_BAUD_RATE / PCLK_FREQ);

	/* set frequency */
	GR_UART_NCO(uart) = setting;

	/*
	 * Interrupt when RX fifo has anything, when TX fifo <= half
	 * empty and reset (clear) both FIFOs
	 */
	GR_UART_FIFO(uart) = 0x63;

	/* enable RX interrupts in block */
	/* Note: doesn't do anything unless turned on in NVIC */
	GR_UART_ICTRL(uart) = 0x02;

#if USE_UART_INTERRUPTS
	/* Enable interrupts for UART */
	uartn_enable_interrupt(uart);
#endif
}
