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

int uart_init_done(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 1;
}

void uart_tx_start(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

void uart_tx_stop(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

void uart_tx_flush(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

int uart_tx_ready(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

int uart_tx_in_progress(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

int uart_rx_available(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return 0;
}

void uart_write_char(char c)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

int uart_read_char(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
	return '-';
}

void uart_disable_interrupt(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

void uart_enable_interrupt(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}

void uart_init(void)
{
	/* TODO(crosbug.com/p/23575): IMPLEMENT ME ! */
}
