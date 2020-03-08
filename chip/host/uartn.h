/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_UARTN_H
#define __CROS_EC_UARTN_H

#include "uart.h"

/**
 * Flush the transmit FIFO.
 */
#define uartn_tx_flush(uart)	do {	uart_tx_flush(); \
					uart = uart ^ 0; \
				} while (0)


/**
 * Send a character to the UART data register.
 * @param c		Character to send.
 */
#define uartn_write_char(uart, ch)	do {	uart_write_char(ch); \
						uart = uart ^ 0; \
					} while (0)


#endif  /* __CROS_EC_UARTN_H */
