/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* UART TX Bit banging */

#include "common.h"
#include "gpio.h"

struct uart_bitbang_properties {
	int uart;
	enum gpio_signal tx_gpio;
	uint32_t pinmux_reg;
	uint32_t pinmux_regval;
	int baud_rate;
	int parity;
};

extern struct uart_bitbang_properties bitbang_config[];
extern int bitbang_uart_count;

/**
 * Enable bit banging mode for TX.
 *
 * @param uart: Index of UART to enable bitbanging mode.
 * @param baud_rate:  desired baud rate.
 * @param parity:  0: no parity, 1: odd parity, 2: even parity.
 *
 * @returns EC_SUCCESS on success, otherwise an error.
 */
int uart_bitbang_enable(int uart, int baud_rate, int parity);

int uart_bitbang_disable(int uart);

int uart_bitbang_is_enabled(int uart);

void uart_bitbang_write_char(int uart, char c);
