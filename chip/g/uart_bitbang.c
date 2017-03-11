/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "pmu.h"
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
static uint32_t bitbang_enabled;

static int find_config_idx(int uart) {
	int i;
	for (i = 0; i < bitbang_uart_count; i++) {
		if (uart == bitbang_config[i].uart)
			return i;
	}

	return -1;
}

int uart_bitbang_is_enabled(int uart)
{
	int is_enabled = 0;
	int index;

	index = find_config_idx(uart);
	if (index != -1)
		is_enabled = bitbang_enabled & (1 << index);

	return !!is_enabled;
}

int uart_bitbang_enable(int uart, int baud_rate, int parity)
{
	int tbl_index = -1;

	tbl_index = find_config_idx(uart);

	if (tbl_index == -1) {
		CPRINTF("bit bang config not found for UART%d", uart);
		return EC_ERROR_INVAL;
	}

	/* Check desired properties. */
	if (!IS_BAUD_RATE_SUPPORTED(baud_rate)) {
		CPRINTF("Err: invalid baud rate (%d)", baud_rate);
		return EC_ERROR_INVAL;
	}
	bitbang_config[tbl_index].baud_rate = baud_rate;

	switch (parity) {
	case 0:
	case 1:
	case 2:
		break;

	default:
		CPRINTF("Err: invalid parity '%d'. (0:N, 1:O, 2:E)", parity);
		return EC_ERROR_INVAL;
	};
	bitbang_config[tbl_index].parity = parity;

	/* Select the GPIO instead of the UART block. */
	REG32(bitbang_config[tbl_index].pinmux_reg) =
		bitbang_config[tbl_index].pinmux_regval;
	gpio_set_flags(bitbang_config[tbl_index].tx_gpio, GPIO_OUT_HIGH);

	/*
	 * Ungate the microsend timer so that we can use it.  This is needed for
	 * accurate framing if using faster baud rates.
	 */
	pmu_clock_en(PERIPH_TIMEUS);

	bitbang_enabled |= (1 << tbl_index);
	ccprintf("successfully enabled\n");
	cflush();
	return EC_SUCCESS;
}

int uart_bitbang_disable(int uart)
{
	int tbl_index;
	if (!uart_bitbang_is_enabled(uart))
		return EC_SUCCESS;

	/*
	 * This is safe because if the UART was not in the config table, we
	 * would have already returned.
	 */
	tbl_index = find_config_idx(uart);
	bitbang_enabled &= ~(1 << tbl_index);
	gpio_reset(bitbang_config[tbl_index].tx_gpio);

	if (!bitbang_enabled)
		pmu_clock_dis(PERIPH_TIMEUS);

	/* Reconnect the GPIO to the UART block. */
	uartn_tx_connect(uart);
	return EC_SUCCESS;
}

static void wait_us(uint32_t us)
{
	/* On your marks... */
	GR_TIMEUS_EN(0) = 0;
	GR_TIMEUS_CUR_MAJOR(0) = 0;
	GR_TIMEUS_CUR_MINOR(0) = 0;
	/* Get set... */
	GR_TIMEUS_MAXVAL(0) = us;
	GR_TIMEUS_DIVIDER(0) = 1;
	GR_TIMEUS_ONESHOT_MODE(0) = 1;
	/* Go! */
	GR_TIMEUS_EN(0) = 1;

	while (GR_TIMEUS_CUR_MAJOR(0) < us)
		;

	GR_TIMEUS_EN(0) = 0;
}

void uart_bitbang_write_char(int uart, char c)
{
	uint32_t bit_period_us;
	enum gpio_signal s;
	int val;
	int ones;
	int i;
	int tbl_index;
	void (*delay)(uint32_t us);

	if (!uart_bitbang_is_enabled(uart))
		return;

	tbl_index = find_config_idx(uart);
	bit_period_us = (1 * SECOND) / bitbang_config[tbl_index].baud_rate;
	s = bitbang_config[tbl_index].tx_gpio;

	if (bitbang_config[tbl_index].baud_rate == 115200)
		delay = wait_us;
	else
		delay = udelay;

	/* Start bit. */
	gpio_set_level(s, 1);
	delay(bit_period_us);
	gpio_set_level(s, 0);
	delay(bit_period_us);

	/* 8 data bits. */
	for (i = 0; i < 8; i++) {
		val = (c & (1 << i));
		/* Count 1's in order to handle parity bit. */
		if (val)
			ones++;
		gpio_set_level(s, val);
		delay(bit_period_us);
	}

	/* Optional parity. */
	switch (bitbang_config[tbl_index].parity) {
	case 1: /* odd parity */
		if (ones & 0x1)
			gpio_set_level(s, 0);
		else
			gpio_set_level(s, 1);
		delay(bit_period_us);
		break;

	case 2: /* even parity */
		if (ones & 0x1)
			gpio_set_level(s, 1);
		else
			gpio_set_level(s, 0);
		delay(bit_period_us);
		break;

	case 0: /* no parity */
	default:
		break;
	};

	/* 1 stop bit. */
	gpio_set_level(s, 1);
	delay(bit_period_us);
}

int command_bitbang_test(int argc, char **argv)
{
	uartn_write_char(2, 'a');
	uartn_write_char(2, 'b');
	uartn_write_char(2, 'c');
	uartn_write_char(2, '\n');
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bbtest, command_bitbang_test,
			"",
			"writes abc\\n");
