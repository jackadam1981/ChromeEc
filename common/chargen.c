/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "console.h"
#include "timer.h"
#include "uart.h"
#include "usb_console.h"
#include "util.h"
#include "watchdog.h"

#ifndef SECTION_IS_RO

#ifdef CONFIG_ZEPHYR
#include <drivers/uart.h>
#include <shell/shell_uart.h>
#include <kernel.h>

/* The UART device used as output. */
static const struct device *chargen_uart_dev;

/*
 * Stop the shell process and block the interrupt to get exclusive access to
 * the UART port.
 */
static void shell_suspend(void)
{
	const struct shell *shell;

	shell = shell_backend_uart_get_ptr();

	uart_irq_rx_disable(chargen_uart_dev);
	uart_irq_tx_disable(chargen_uart_dev);
	shell_stop(shell);
}

/* Resume the UART operation. */
static void shell_resume(void)
{
	const struct shell *shell;

	shell = shell_backend_uart_get_ptr();

	shell_start(shell);
	uart_irq_rx_enable(chargen_uart_dev);
	uart_irq_tx_enable(chargen_uart_dev);
}

/* Get one character from the UART port, non blocking. */
int uart_getc(void)
{
	uint8_t c;
	int ret;

	ret = uart_poll_in(chargen_uart_dev, &c);
	if (ret < 0)
		return ret;
	return c;
}

/* Check if the output FIFO on the UART port is full. */
int uart_buffer_full(void)
{
	return !uart_irq_tx_ready(chargen_uart_dev);
}
#endif  /* CONFIG_ZEPHYR */

/*
 * Microseconds time to drain entire UART_TX console buffer at 115200 b/s, 10
 * bits per character.
 */
#define BUFFER_DRAIN_TIME_US (1000000UL * 10 * CONFIG_UART_TX_BUF_SIZE         \
				/ CONFIG_UART_BAUD_RATE)

/*
 * Generate a stream of characters on the UART (and USB) console.
 *
 * The stream is an ever incrementing pattern of characters from the following
 * set: 0..9A..Za..z.
 *
 * The two optional integer command line arguments work as follows:
 *
 * argv[1] - reset the pattern after this many characters have been printed.
 *           Setting this value to the width of the terminal window results
 *           in a very regular stream showing on the terminal, where it is
 *           easy to observe disruptions.
 * argv[2] - limit number of printed characters to this amount. If not
 *           specified - keep printing indefinitely.
 *
 * Hitting 'x' on the keyboard stops the generator.
 */
static int command_chargen_run(int argc, char **argv)
{
	int wrap_value = 0;
	int wrap_counter = 0;
	uint8_t c;
	uint32_t seq_counter = 0;
	uint32_t seq_number = 0;
	timestamp_t prev_watchdog_time;

	int (*putc_)(int c) = uart_putc;
	int (*tx_is_blocked_)(void) = uart_buffer_full;

	while (uart_getc() != -1 || usb_getc() != -1)
		; /* Drain received characters, if any. */

	if (argc > 1)
		wrap_value = atoi(argv[1]);

	if (argc > 2)
		seq_number = atoi(argv[2]);

#if defined(CONFIG_USB_CONSOLE) || defined(CONFIG_USB_CONSOLE_STREAM)
	if (argc > 3) {
		if (memcmp(argv[3], "usb", 3))
			return EC_ERROR_PARAM3;

		putc_ = usb_putc;
		tx_is_blocked_ = usb_console_tx_blocked;
	}
#endif

	c = '0';
	prev_watchdog_time = get_time();
	while (uart_getc() != 'x' && usb_getc() != 'x') {
		timestamp_t current_time;

		while (tx_is_blocked_()) {
			/*
			 * Let's let other tasks run for a bit while buffer is
			 * being drained a little.
			 */
			usleep(BUFFER_DRAIN_TIME_US/10);

			current_time = get_time();

			if ((current_time.val - prev_watchdog_time.val) <
			    (CONFIG_WATCHDOG_PERIOD_MS * 1000 / 2))
				continue;

			watchdog_reload();
			prev_watchdog_time.val = current_time.val;
		}

		putc_(c++);

		if (seq_number && (++seq_counter == seq_number))
			break;

		if (wrap_value && (++wrap_counter == wrap_value)) {
			c = '0';
			wrap_counter = 0;
			continue;
		}

		if (c == ('z' + 1))
			c = '0';
		else if (c == ('Z' + 1))
			c = 'a';
		else if (c  == ('9' + 1))
			c = 'A';
	}

	putc_('\n');
	return EC_SUCCESS;
}

/*
 * Wrapper for the actual chargen command to pause the shell and get exclusive
 * use of the UART when running in Zephyr.
 */
static int command_chargen(int argc, char **argv)
{
#ifdef CONFIG_ZEPHYR
	int ret;

	chargen_uart_dev = device_get_binding(CONFIG_UART_SHELL_ON_DEV_NAME);
	if (chargen_uart_dev == NULL)
		return EC_ERROR_INVAL;

	shell_suspend();
	ret = command_chargen_run(argc, argv);
	shell_resume();

	return ret;
#else
	return command_chargen_run(argc, argv);
#endif
}

DECLARE_SAFE_CONSOLE_COMMAND(chargen, command_chargen,
#if defined(CONFIG_USB_CONSOLE) || defined(CONFIG_USB_CONSOLE_STREAM)
			     "[seq_length [num_chars [usb]]]",
#else
			     "[seq_length [num_chars]]",
#endif
			     "Generate a constant stream of characters on the "
			     "UART console,\nrepeating every 'seq_length' "
			     "characters, up to 'num_chars' total."
	);
#endif  /* !SECTION_IS_RO */
