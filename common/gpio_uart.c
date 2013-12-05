/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* GPIO UART functionality for Chrome EC */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define GPIO_UART_IN_PIN GPIO_035
#define GPIO_UART_OUT_PIN GPIO_036
#define BAUD 9600
#define BIT_PERIOD (1000000 / BAUD)

enum char_state {
	CHAR_STATE_NONE = 0,
	CHAR_STATE_RECV,
	CHAR_STATE_READY
};

#define FIFO_LEN 32 /* power of 2 */
#define FIFO_MASK (FIFO_LEN - 1)
#define FIFO_NEXT(x) (((x) + 1) & FIFO_MASK)
struct gpio_uart_buf {
	uint8_t state;
	char decoded;
	uint8_t len;
	uint8_t data[9];
};
static struct gpio_uart_buf rx_buf_fifo[FIFO_LEN];
static uint8_t rx_head, rx_tail;

static char tx_buf_fifo[FIFO_LEN];
static uint8_t tx_head, tx_tail;

static uint8_t tx_started;

static timestamp_t cst;

void gpio_uart_raw_send_char(int c)
{
	int i;
	timestamp_t st;
	int32_t d;
	c = (c << 1) | (1 << 9);
	st = get_time();
	for (i = 0; i < 10; ++i) {
		gpio_set_level(GPIO_UART_OUT_PIN, c & 1);
		d = MAX(st.val + BIT_PERIOD * (i + 1) - get_time().val, 0);
		if (d)
			udelay(d);
		c >>= 1;
	}
}

int gpio_uart_tx_ready(void)
{
	return FIFO_NEXT(tx_head) != tx_tail;
}

void gpio_uart_write_char(int c)
{
	while (!gpio_uart_tx_ready())
		msleep(10); /* TODO: what to do when full? */
	tx_buf_fifo[tx_head] = c;
	tx_head = FIFO_NEXT(tx_head);
}

void gpio_uart_tx_flush(void)
{
	while (tx_head != tx_tail) {
		gpio_uart_raw_send_char(tx_buf_fifo[tx_tail]);
		tx_tail = FIFO_NEXT(tx_tail);
	}
}

void gpio_uart_tx_stop(void)
{
	tx_started = 0;
}

void gpio_uart_tx_start(void)
{
	tx_started = 1;
}

static void decode_char(struct gpio_uart_buf *b)
{
	char c = '\0';
	int i, j = 0;
	uint8_t v = 0;
	for (i = 0; i < b->len; ++i) {
		for (; j < b->data[i]; ++j)
			c |= v << j;
		v ^= 1;
	}
	b->state = CHAR_STATE_READY;
	b->decoded = c;
}

void process_rx_raw_data(void)
{
	int i;
	if (rx_buf_fifo[rx_head].state == CHAR_STATE_RECV &&
	    get_time().val - cst.val > BIT_PERIOD * 15) /* Receive timed out */
		rx_head = FIFO_NEXT(rx_head);
	for (i = rx_tail; i != rx_head; i = FIFO_NEXT(i))
		if (rx_buf_fifo[i].state != CHAR_STATE_READY)
			decode_char(rx_buf_fifo + i);
}

int gpio_uart_read_char(void)
{
	char ret;
	if (rx_head == rx_tail)
		return -1;
	ret = rx_buf_fifo[rx_tail].decoded;
	rx_buf_fifo[rx_tail].state = CHAR_STATE_NONE;
	rx_tail = FIFO_NEXT(rx_tail);
	return ret;
}

int gpio_uart_rx_availabe(void)
{
	return rx_head != rx_tail;
}

static int rx_buf_full(void)
{
	return FIFO_NEXT(rx_head) == rx_tail;
}

static void gpio_uart_init(void)
{
	gpio_enable_interrupt(GPIO_UART_IN_PIN);
	rx_buf_fifo[0].state = CHAR_STATE_NONE;
}
DECLARE_HOOK(HOOK_INIT, gpio_uart_init, HOOK_PRIO_DEFAULT);

static void check_char(void)
{
	gpio_uart_tx_flush();
	process_rx_raw_data();

	/* Dump for debugging */
	while (gpio_uart_rx_availabe())
		ccprintf("%c", gpio_uart_read_char());
}
DECLARE_HOOK(HOOK_TICK, check_char, HOOK_PRIO_DEFAULT);

void gpio_uart_interrupt(enum gpio_signal s)
{
	uint32_t d = get_time().val - cst.val;
	d = (d + (BIT_PERIOD / 2)) / BIT_PERIOD - 1;
	if (rx_buf_fifo[rx_head].state == CHAR_STATE_NONE) {
		rx_buf_fifo[rx_head].state = CHAR_STATE_RECV;
		rx_buf_fifo[rx_head].len = 0;
		cst = get_time();
	} else if (d > 8) {
		cst = get_time();
		if (rx_buf_full())
			rx_tail = FIFO_NEXT(rx_tail);
		rx_head = FIFO_NEXT(rx_head);
		rx_buf_fifo[rx_head].state = CHAR_STATE_RECV;
		rx_buf_fifo[rx_head].len = 0;
	} else {
		rx_buf_fifo[rx_head].data[rx_buf_fifo[rx_head].len++] = d;
	}
}

static int cmd_gpio_uart(int argc, char **argv)
{
	char *p;
	if (argc != 2)
		return EC_ERROR_UNKNOWN;
	p = argv[1];
	while (*p)
		gpio_uart_write_char(*(p++));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gu, cmd_gpio_uart, NULL, NULL, NULL);
