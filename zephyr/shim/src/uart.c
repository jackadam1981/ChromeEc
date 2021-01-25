/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <drivers/uart.h>
#include <stdbool.h>
#include <zephyr.h>

#include "uart.h"

static const struct device *uart_dev;

void uart_init(void)
{
	 uart_dev = device_get_binding(CONFIG_PLATFORM_EC_UART_NAME);
}

int uart_init_done(void)
{
	return !!uart_dev;
}

void uart_write_char(char c)
{
	uart_poll_out(uart_dev, c);
}

#ifdef CONFIG_UART_INTERRUPT_DRIVEN
void uart_rx_dma_start(char *dest, int len)
{
	uart_rx_buf_rsp(uart_dev, (uint8_t *)buf, len);
}

int uart_rx_dma_head(void)
{

}
#else /* !CONFIG_UART_INTERRUPT_DRIVEN */
static unsigned char char_read;

int uart_rx_available(void)
{
	if (uart_poll_in(uart_dev, &char_read)) {
		/* Char not available */
		return false;
	}
	return true;
}

int uart_read_char(void)
{
	return char_read;
}
#endif /* !CONFIG_UART_INTERRUPT_DRIVEN */

void uart_tx_start(void)
{
	
}
