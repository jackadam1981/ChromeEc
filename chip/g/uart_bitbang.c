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
#include "task.h"
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

#define RX_BUF_SIZE 8
#define DISCARD_LOG 8

#define BUF_NEXT(idx) ((idx+1) % RX_BUF_SIZE)

#define TIMEUS_CLK_FREQ 24 /* units: MHz */

/* Bitmask of UARTs with bit banging enabled. */
static uint32_t bitbang_enabled;
/* TODO(aaboagye): just a hack for now. */
static int rx_buffers[1][RX_BUF_SIZE];
static int parity_err_discard[1][DISCARD_LOG];
static int parity_discard_idx;
static int stop_bit_discard[1][DISCARD_LOG];
static int stop_bit_discard_idx;

/* debug counters */
static int read_char_cnt;
static int rx_buff_inserted_cnt;
static int rx_buff_rx_char_cnt;
static int stop_bit_err_cnt;
static int parity_err_cnt;

/* Current bitbang context */
static int tx_pin;
static int rx_pin;
static uint32_t bit_period_ticks;
static uint8_t set_parity;

static int find_config_idx(int uart)
{
	int i;

	for (i = 0; i < bitbang_uart_count; i++) {
		if (uart == bitbang_config[i].uart)
			return i;
	}

	return -1;
}

int uart_bitbang_is_enabled(int uart)
{
	int index = find_config_idx(uart);

	return ((index >= 0) && (bitbang_enabled & (1 << index)));
}

int uart_bitbang_enable(int uart, int baud_rate, int parity)
{
	int tbl_index = -1;

	/* We only want to bit bang 1 UART at a time. */
	if (bitbang_enabled)
		return EC_ERROR_BUSY;

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
	bitbang_config[tbl_index].head_tail_parity |= parity;

	/* Select the GPIOs instead of the UART block. */
	uartn_tx_disconnect(bitbang_config[tbl_index].uart);
	REG32(bitbang_config[tbl_index].tx_pinmux_reg) =
		bitbang_config[tbl_index].tx_pinmux_regval;
	gpio_set_flags(bitbang_config[tbl_index].tx_gpio, GPIO_OUT_HIGH);
	REG32(bitbang_config[tbl_index].rx_pinmux_reg) =
		bitbang_config[tbl_index].rx_pinmux_regval;
	gpio_set_flags(bitbang_config[tbl_index].rx_gpio, GPIO_INPUT);

	/*
	 * Ungate the microsecond timer so that we can use it.  This is needed
	 * for accurate framing if using faster baud rates.
	 */
	pmu_clock_en(PERIPH_TIMEUS);
	GR_TIMEUS_EN(0) = 0;
	GR_TIMEUS_MAXVAL(0) = 0xFFFFFFFF;
	GR_TIMEUS_EN(0) = 1;

	/* Save context information. */
	tx_pin = bitbang_config[tbl_index].tx_gpio;
	rx_pin = bitbang_config[tbl_index].rx_gpio;
	bit_period_ticks = TIMEUS_CLK_FREQ *
		((1 * SECOND) / bitbang_config[tbl_index].baud_rate);
	set_parity = EXTRACT_PARITY(bitbang_config[tbl_index].head_tail_parity);

	/* Register the function pointers. */
	uartn_funcs[uart]._rx_available = _uart_bitbang_rx_available;
	uartn_funcs[uart]._write_char = _uart_bitbang_write_char;
	uartn_funcs[uart]._read_char = _uart_bitbang_read_char;

	bitbang_enabled |= (1 << tbl_index);
	gpio_enable_interrupt(bitbang_config[tbl_index].rx_gpio);
	ccprintf("successfully enabled\n");
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
	gpio_reset(bitbang_config[tbl_index].rx_gpio);

	/* Unregister the function pointers. */
	uartn_funcs[uart]._rx_available = _uartn_rx_available;
	uartn_funcs[uart]._write_char = _uartn_write_char;
	uartn_funcs[uart]._read_char = _uartn_read_char;

	if (!bitbang_enabled)
		pmu_clock_dis(PERIPH_TIMEUS);

	/* Reconnect the GPIO to the UART block. */
	gpio_disable_interrupt(bitbang_config[tbl_index].rx_gpio);
	uartn_tx_connect(uart);
	return EC_SUCCESS;
}

static void wait_ticks(uint32_t ticks)
{
	uint32_t t0 = GR_TIMEUS_CUR_MAJOR(0);

	while ((GR_TIMEUS_CUR_MAJOR(0) - t0) < ticks)
		;
}

void uart_bitbang_write_char(int uart, char c)
{
	int val;
	int ones;
	int i;

	if (!uart_bitbang_is_enabled(uart))
		return;

	interrupt_disable();

	/* Start bit. */
	gpio_set_level(tx_pin, 0);
	wait_ticks(bit_period_ticks);

	/* 8 data bits. */
	ones = 0;
	for (i = 0; i < 8; i++) {
		val = (c & (1 << i));
		/* Count 1's in order to handle parity bit. */
		if (val)
			ones++;
		gpio_set_level(tx_pin, val);
		wait_ticks(bit_period_ticks);
	}

	/* Optional parity. */
	switch (set_parity) {
	case 1: /* odd parity */
		if (ones & 0x1)
			gpio_set_level(tx_pin, 0);
		else
			gpio_set_level(tx_pin, 1);
		wait_ticks(bit_period_ticks);
		break;

	case 2: /* even parity */
		if (ones & 0x1)
			gpio_set_level(tx_pin, 1);
		else
			gpio_set_level(tx_pin, 0);
		wait_ticks(bit_period_ticks);
		break;

	case 0: /* no parity */
	default:
		break;
	};

	/* 1 stop bit. */
	gpio_set_level(tx_pin, 1);
	wait_ticks(bit_period_ticks);
	interrupt_enable();
}

int uart_bitbang_receive_char(int uart)
{
	int tbl_idx;
	uint8_t rx_char;
	int i;
	int rv;
	int ones;
	int parity_bit;
	int stop_bit;
	uint8_t head;
	uint8_t tail;

	/* Disable interrupts so that we aren't interrupted. */
	interrupt_disable();
	rx_buff_rx_char_cnt++;
	rv = EC_SUCCESS;

	tbl_idx = find_config_idx(uart);
	rx_char = 0;

	/* Wait 1 bit period for the start bit. */
	wait_ticks(bit_period_ticks);

	/* 8 data bits. */
	ones = 0;
	for (i = 0; i < 8; i++) {
		if (gpio_get_level(rx_pin)) {
			ones++;
			rx_char |= (1 << i);
		}
		wait_ticks(bit_period_ticks);
	}

	/* optional parity or stop bit. */
	parity_bit = gpio_get_level(rx_pin);
	if (set_parity) {
		wait_ticks(bit_period_ticks);
		stop_bit = gpio_get_level(rx_pin);
	} else {
		/* If there's no parity, that _was_ the stop bit. */
		stop_bit = parity_bit;
	}

	/* Check the parity if necessary. */
	switch (set_parity) {
	case 2: /* even parity */
		if (ones & 0x1)
			rv = parity_bit ? EC_SUCCESS : EC_ERROR_CRC;
		else
			rv = parity_bit ? EC_ERROR_CRC : EC_SUCCESS;
		break;

	case 1: /* odd parity */
		if (ones & 0x1)
			rv = parity_bit ? EC_ERROR_CRC : EC_SUCCESS;
		else
			rv = parity_bit ? EC_SUCCESS : EC_ERROR_CRC;
		break;

	case 0:
	default:
		break;
	}

	if (rv != EC_SUCCESS) {
		parity_err_cnt++;
		parity_err_discard[tbl_idx][parity_discard_idx] = rx_char;
		parity_discard_idx = (parity_discard_idx + 1) % DISCARD_LOG;
	}

	/* Check that the stop bit is valid. */
	if (stop_bit != 1) {
		rv = EC_ERROR_CRC;
		stop_bit_err_cnt++;
		stop_bit_discard[tbl_idx][stop_bit_discard_idx] = rx_char;
		stop_bit_discard_idx = (stop_bit_discard_idx + 1) % DISCARD_LOG;
	}

	if (rv != EC_SUCCESS) {
		interrupt_enable();
		return rv;
	}

	/* Place the received char in the RX buffer. */
	head = EXTRACT_HEAD(bitbang_config[tbl_idx].head_tail_parity);
	tail = EXTRACT_TAIL(bitbang_config[tbl_idx].head_tail_parity);
	if (BUF_NEXT(tail) != head) {
		rx_buffers[tbl_idx][tail] = rx_char;
		bitbang_config[tbl_idx].head_tail_parity &= ~TAIL_MASK;
		bitbang_config[tbl_idx].head_tail_parity |=
			(BUF_NEXT(tail) << TAIL_OFFSET);
		rx_buff_inserted_cnt++;
	}

	interrupt_enable();
	return EC_SUCCESS;
}

int uart_bitbang_read_char(int uart)
{
	int c;
	int i;
	uint8_t head;

	i = find_config_idx(uart);
	if (i == -1)
		return 0;

	head = EXTRACT_HEAD(bitbang_config[i].head_tail_parity);
	c = rx_buffers[i][head];

	if (head != EXTRACT_TAIL(bitbang_config[i].head_tail_parity)) {
		bitbang_config[i].head_tail_parity &= ~HEAD_MASK;
		bitbang_config[i].head_tail_parity |=
			(BUF_NEXT(head) << HEAD_OFFSET);
	}

	read_char_cnt++;
	return c;
}

int uart_bitbang_is_char_available(int uart)
{
	int i = find_config_idx(uart);
	uint8_t htp;

	if (i == -1)
		return 0;

	htp = bitbang_config[i].head_tail_parity;
	return EXTRACT_HEAD(htp) != EXTRACT_TAIL(htp);
}

static int command_bitbang_dump_stats(int argc, char **argv)
{
	int i;

	if (argc == 2) {
		/* Clear the counters. */
		if (!strncasecmp(argv[1], "clear", 5)) {
			parity_err_cnt = 0;
			stop_bit_err_cnt = 0;
			rx_buff_rx_char_cnt = 0;
			read_char_cnt = 0;
			rx_buff_inserted_cnt = 0;
			return EC_SUCCESS;
		}
		return EC_ERROR_PARAM1;
	}

	ccprintf("Errors:\n");
	ccprintf("%d Parity Errors\n", parity_err_cnt);
	ccprintf("%d Stop Bit Errors\n", stop_bit_err_cnt);
	ccprintf("Buffer info\n");
	ccprintf("%d received\n", rx_buff_rx_char_cnt);
	ccprintf("%d chars inserted\n", rx_buff_inserted_cnt);
	ccprintf("%d chars read\n", read_char_cnt);
	ccprintf("Contents\n");
	ccprintf("[");
	for (i = 0; i < RX_BUF_SIZE; i++)
		ccprintf(" %02x ", rx_buffers[0][i] & 0xFF);
	ccprintf("]\n");
	ccprintf("head: %d\ntail: %d\n",
		 EXTRACT_HEAD(bitbang_config[0].head_tail_parity),
		 EXTRACT_TAIL(bitbang_config[0].head_tail_parity));
	ccprintf("Discards\nparity: ");
	ccprintf("[");
	for (i = 0; i < DISCARD_LOG; i++)
		ccprintf(" %02x ", parity_err_discard[0][i] & 0xFF);
	ccprintf("]\n");
	ccprintf("idx: %d\n", parity_discard_idx);
	ccprintf("stop bit: ");
	ccprintf("[");
	for (i = 0; i < DISCARD_LOG; i++)
		ccprintf(" %02x ", stop_bit_discard[0][i] & 0xFF);
	ccprintf("]\n");
	ccprintf("idx: %d\n", stop_bit_discard_idx);
	ccprintf("htp: 0x%08x\n", bitbang_config[0].head_tail_parity);
	cflush();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bbstats, command_bitbang_dump_stats,
			"",
			"dumps bitbang stats");
