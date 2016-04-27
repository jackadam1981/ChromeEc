/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common code to do UART buffering and printing */

#include <stdarg.h>

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "printf.h"
#include "queue.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/*
 * Interval between rechecking the receive DMA head pointer, after a character
 * of input has been detected by the normal tick task.  There will be
 * CONFIG_UART_RX_DMA_RECHECKS rechecks between this tick and the next tick.
 */
#define RX_DMA_RECHECK_INTERVAL (HOOK_TICK_INTERVAL /			\
				 (CONFIG_UART_RX_DMA_RECHECKS + 1))

/* Transmit and receive buffers */
static const struct queue tx = QUEUE_NULL(CONFIG_UART_TX_BUF_SIZE, char);
static const struct queue rx = QUEUE_NULL(CONFIG_UART_RX_BUF_SIZE, char);

/*
 * Queue:                       head      <= tail
 * Host:  tail - queue.units <= host_head <= tail
 */

/**
 * Put a single character into the transmit buffer.
 *
 * Does not enable the transmit interrupt; assumes that happens elsewhere.
 *
 * @param context	Context; ignored.
 * @param c		Character to write.
 * @return 0 if the character was transmitted, 1 if it was dropped.
 */
static int __tx_char(void *context, int c)
{
	char temp = c;

	/* Do newline to CRLF translation */
	if (c == '\n' && __tx_char(NULL, '\r'))
		return 1;

#if defined CONFIG_POLLING_UART
	uart_write_char(c);
	return 0;
#else
	return !queue_add_unit(&tx, &temp);
#endif
}

#ifdef CONFIG_UART_TX_DMA

/**
 * Process UART output via DMA
 */
void uart_process_output(void)
{
	/* Queue chunk for current DMA transfer */
	static struct queue_chunk chunk;

	/*
	 * If we are ready to start a DMA transfer, advance the queue head by
	 * the size of the previous transfer (If there was no previous transfer
	 * because this is the first call to uart_process_output the call to
	 * queue_advance_head will have no effect), and start a new transfer if
	 * needed.
	 */
	if (uart_tx_dma_ready()) {
		queue_advance_head(&tx, chunk.length);

		chunk = queue_get_read_chunk(&tx);

		if (chunk.length)
			uart_tx_dma_start(chunk.buffer, chunk.length);
		else
			uart_tx_stop();
	}
}

#else /* !CONFIG_UART_TX_DMA */

void uart_process_output(void)
{
	char c;

	/* Copy output from buffer until TX fifo full or output buffer empty */
	while (uart_tx_ready() && queue_remove_unit(&tx, &c))
		uart_write_char(c);

	/* If output buffer is empty, disable transmit interrupt */
	if (queue_is_empty(&tx))
		uart_tx_stop();
}

#endif /* !CONFIG_UART_TX_DMA */

#ifdef CONFIG_UART_RX_DMA
#ifdef CONFIG_UART_INPUT_FILTER  /* TODO(crosbug.com/p/36745): */
#error "Filtering the UART input with DMA enabled is NOT SUPPORTED!"
#endif

void uart_process_input(void);
DECLARE_DEFERRED(uart_process_input);

void uart_process_input(void)
{
	static int fast_rechecks;
	int cur_head = rx_buf_head;

	/* Update receive buffer head from current DMA receive pointer */
	rx_buf_head = uart_rx_dma_head();

	if (rx_buf_head != cur_head) {
		console_has_input();
		fast_rechecks = CONFIG_UART_RX_DMA_RECHECKS;
	}

	/*
	 * Input is checked once a tick when the console is idle.  When input
	 * is received, check more frequently for a bit, so that the console is
	 * more responsive.
	 */
	if (fast_rechecks) {
		fast_rechecks--;
		hook_call_deferred(&uart_process_input_data,
				   RX_DMA_RECHECK_INTERVAL);
	}
}
DECLARE_HOOK(HOOK_TICK, uart_process_input, HOOK_PRIO_DEFAULT);

static void uart_rx_dma_init(void)
{
	/* Start receiving */
	uart_rx_dma_start(rx.buffer, rx.buffer_units);
}
DECLARE_HOOK(HOOK_INIT, uart_rx_dma_init, HOOK_PRIO_DEFAULT);

#else /* !CONFIG_UART_RX_DMA */

void uart_process_input(void)
{
	int got_input = 0;

	/* Copy input from buffer until RX fifo empty */
	while (uart_rx_available()) {
		int c = uart_read_char();

#ifdef CONFIG_UART_INPUT_FILTER
		/* Intercept the input before it goes to the console */
		if (uart_input_filter(c))
			continue;
#endif

		got_input |= queue_add_unit(&rx, &c);
	}

	if (got_input)
		console_has_input();
}

#endif /* !CONFIG_UART_RX_DMA */

int uart_putc(int c)
{
	int rv = __tx_char(NULL, c);

	uart_tx_start();

	return rv ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int uart_puts(const char *outstr)
{
	/* Put all characters in the output buffer */
	while (*outstr) {
		if (__tx_char(NULL, *outstr++) != 0)
			break;
	}

	uart_tx_start();

	/* Successful if we consumed all output */
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int uart_vprintf(const char *format, va_list args)
{
	int rv = vfnprintf(__tx_char, NULL, format, args);

	uart_tx_start();

	return rv;
}

int uart_printf(const char *format, ...)
{
	int rv;
	va_list args;

	va_start(args, format);
	rv = uart_vprintf(format, args);
	va_end(args);
	return rv;
}

void uart_flush_output(void)
{
	/* If UART not initialized ignore flush request. */
	if (!uart_init_done())
		return;

	/* Loop until buffer is empty */
	while (!queue_is_empty(&tx)) {
		if (in_interrupt_context()) {
			/*
			 * Explicitly process UART output, since the UART
			 * interrupt may not be able to preempt the interrupt
			 * we're in now.
			 */
			uart_process_output();
		} else {
			/*
			 * It's possible we switched from a previous context
			 * which was doing a printf() or puts() but hadn't
			 * enabled the UART interrupt.  Check if the interrupt
			 * is disabled, and if so, re-enable and trigger it.
			 * Note that this check is inside the while loop, so
			 * we'll be safe even if the context switches away from
			 * us to another partial printf() and back.
			 */
			uart_tx_start();
		}
	}

	/* Wait for transmit FIFO empty */
	uart_tx_flush();
}

int uart_getc(void)
{
	char c;

	if (queue_remove_unit(&rx, &c))
		return c;

	return -1;
}

int uart_buffer_empty(void)
{
	return queue_is_empty(&tx);
}

/*****************************************************************************/
/* Host commands */

static int host_command_console_snapshot(struct host_cmd_handler_args *args)
{
	/*
	 * There is nothing to do here anymore, the secondary queue always
	 * maintains the correct section of the primary queue buffer to read.
	 */
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CONSOLE_SNAPSHOT,
		     host_command_console_snapshot,
		     EC_VER_MASK(0));

static int host_command_console_read(struct host_cmd_handler_args *args)
{
       const struct ec_params_console_read_v1 *p = args->params;

       /*
        * Prior versions of this command only support reading from an entire
        * snapshot, not just the output since the last snapshot.
        */
       switch (args->version == 0 ? CONSOLE_READ_NEXT : p->subcmd)
       {
       case CONSOLE_READ_NEXT:
               args->response_size =
                       queue_peek_units(secondary,
                                        args->response,
                                        0,
                                        args->response_max - 1);
               break;

       case CONSOLE_READ_RECENT:
               args->response_size =
                       queue_remove_units(secondary,
                                          args->response,
                                          args->response_max - 1);
               break;

       default:
               return EC_RES_INVALID_PARAM;
       }

       /* Always Null-terminate */
       ((char *) args->response)[args->response_size++] = '\0';

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_CONSOLE_READ,
		     host_command_console_read,
		     EC_VER_MASK(0) | EC_VER_MASK(1));
