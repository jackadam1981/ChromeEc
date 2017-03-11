/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "registers.h"
#include "timer.h"
#include "uart_bitbang.h"
#include "uartn.h"

#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)

/* Support the "standard" baud rates. */
#define IS_BAUD_RATE_SUPPORTED(rate) \
	((rate == 1200) || (rate == 2400) || (rate == 4800) || (rate == 9600) \
	|| (rate == 19200) || (rate == 38400) || (rate == 57600) || \
	 (rate == 115200))

/* Bitmask of UARTs with bit banging enabled. */
static uint8_t bitbang_enabled;

int uart_bitbang_is_enabled(int uart)
{
	int is_enabled = 0;

	if (uart <= 7 && uart >= 0)
		is_enabled = bitbang_enabled & (1 << uart);

	return !!is_enabled;
}

int uart_bitbang_enable(int uart, int baud_rate, int parity)
{
#ifndef SECTION_IS_RO
	int i;
	int index = -1;

	/* Find the UART. */
	for(i = 0; i < bitbang_uart_count; i++)
		if (bitbang_config[i].uart == uart)
			index = i;

	if (index == -1) {
		CPRINTF("bit bang config not found for UART%d", uart);
		return EC_ERROR_INVAL;
	}

	/* Check desired properties. */
	if (!IS_BAUD_RATE_SUPPORTED(baud_rate)) {
		CPRINTF("Err: invalid baud rate (%d)", baud_rate);
		return EC_ERROR_INVAL;
	}
	bitbang_config[index].baud_rate = baud_rate;

	switch (parity) {
	case 0:
	case 1:
	case 2:
		break;

	default:
		CPRINTF("Err: invalid parity '%d'. (0:N, 1:O, 2:E)", parity);
		return EC_ERROR_INVAL;
	};
	bitbang_config[index].parity = parity;

	/* Select the GPIO instead of the UART block. */
	CPRINTF("wr 0x%08x 0x%08x\n", bitbang_config[index].pinmux_reg,
		bitbang_config[index].pinmux_regval);
	/*** BUG: this causes a reset for some reason. ***/
	REG32(bitbang_config[index].pinmux_reg) =
		bitbang_config[index].pinmux_regval;
	gpio_set_flags(bitbang_config[index].tx_gpio, GPIO_OUT_HIGH);
	/* GWRITE(PINMUX, DIOB5_SEL, 0x14); */

	bitbang_enabled |= (1 << index);
	ccprintf(" successfully enabled\n");
	cflush();
#endif /* !defined(SECTION_IS_RO) */
	return EC_SUCCESS;
}

int uart_bitbang_disable(int uart)
{
	if (!uart_bitbang_is_enabled(uart))
		return EC_SUCCESS;

#ifndef SECTION_IS_RO
	if ((uart > 7) || (uart < 0))
		return EC_ERROR_UNKNOWN;

	bitbang_enabled &= ~(1 << uart);
	gpio_reset(bitbang_config[uart].tx_gpio);

	/* TODO(aaboagye): Fix pinmux, although it's handled by the call below. */

	/* Reconnect the GPIO to the UART block. */
	uartn_tx_connect(uart);
#endif /* !defined(SECTION_IS_RO) */
	return EC_SUCCESS;
}

void uart_bitbang_write_char(int uart, char c)
{
#ifndef SECTION_IS_RO
	uint32_t bit_period_us;
	enum gpio_signal s;
	int val;
	int ones;
	int i;

	if (!uart_bitbang_is_enabled(uart))
		return;

	bit_period_us = (1 * SECOND) / bitbang_config[uart].baud_rate;
	s = bitbang_config[uart].tx_gpio;

	/* Start bit. */
	gpio_set_level(s, 1);
	usleep(bit_period_us);
	gpio_set_level(s, 0);
	usleep(bit_period_us);

	/* 8 data bits. */
	for (i = 0; i < 8; i++) {
		val = (c & (1 << i));
		/* Count 1's in order to handle parity bit. */
		if (val)
			ones++;
		gpio_set_level(s, val);
		usleep(bit_period_us);
	}

	/* Optional parity. */
	switch (bitbang_config[uart].parity) {
	case 1: /* odd parity */
		if (ones & 0x1)
			gpio_set_level(s, 0);
		else
			gpio_set_level(s, 1);
		usleep(bit_period_us);
		break;

	case 2: /* even parity */
		if (ones & 0x1)
			gpio_set_level(s, 1);
		else
			gpio_set_level(s, 0);
		usleep(bit_period_us);
		break;

	case 0: /* no parity */
	default:
		break;
	};

	/* 1 stop bit. */
	gpio_set_level(s, 1);
	usleep(bit_period_us);
#endif /* !defined(SECTION_IS_RO) */
}
