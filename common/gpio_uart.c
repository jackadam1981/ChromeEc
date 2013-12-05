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
#define BIT_PERIOD (1000000/BAUD)

timestamp_t cst;
/*int started;
uint8_t data[10];
int idx;*/

char in_buf[20];
int buf_idx;

enum char_state
{
	CHAR_STATE_NONE = 0,
	CHAR_STATE_RECV,
	CHAR_STATE_READY
};

#define FIFO_LEN 16 /* power of 2 */
#define FIFO_MASK 15
struct gpio_uart_buf
{
	uint8_t state;
	char decoded;
	uint8_t len;
	uint8_t data[9];
} fifo[FIFO_LEN];

uint8_t head, tail;

static void write_char(int c)
{
	int i;
	timestamp_t st;
	int32_t d;
	c = (c << 1) | (1 << 9);
	st = get_time();
	for (i = 0; i < 10; ++i) {
		gpio_set_level(GPIO_UART_OUT_PIN, c & 1);
		d = MAX(st.val + BIT_PERIOD * (i + 1) - get_time().val, 0);
		ASSERT(d);
		if (d)
			udelay(d);
		c >>= 1;
	}
}

void decode_char(struct gpio_uart_buf *b)
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

void process_char(void)
{
	int i;
	if (fifo[head].state == CHAR_STATE_RECV &&
	    get_time().val - cst.val > BIT_PERIOD * 15) /* Receive timed out */
		++head;
	for (i = tail; i != head; i = (i + 1) & FIFO_MASK)
		if (fifo[i].state != CHAR_STATE_READY) {
			decode_char(fifo + i);
		}
}

static char fifo_pop(void)
{
	char ret;
	if (head == tail)
		return 0;
	ret = fifo[tail].decoded;
	fifo[tail].state = CHAR_STATE_NONE;
	tail = (tail + 1) & FIFO_MASK;
	return ret;
}

static void gpio_uart_init(void)
{
	gpio_enable_interrupt(GPIO_UART_IN_PIN);
	fifo[0].state = CHAR_STATE_NONE;
}
DECLARE_HOOK(HOOK_INIT, gpio_uart_init, HOOK_PRIO_DEFAULT);

static void check_char(void)
{
	char c;
	process_char();
	while ((c = fifo_pop()))
		ccprintf("%c", c);
}
DECLARE_HOOK(HOOK_TICK, check_char, HOOK_PRIO_DEFAULT);

void gpio_uart_interrupt(enum gpio_signal s)
{
	uint32_t d = get_time().val - cst.val;
	d = (d + (BIT_PERIOD / 2)) / BIT_PERIOD - 1;
	if (fifo[head].state == CHAR_STATE_NONE) {
		fifo[head].state = CHAR_STATE_RECV;
		fifo[head].len = 0;
		cst = get_time();
	} else if (d > 8) {
		cst = get_time();
		head = (head + 1) & FIFO_MASK;
		fifo[head].state = CHAR_STATE_RECV;
		fifo[head].len = 0;
	} else {
		fifo[head].data[fifo[head].len++] = d;
	}
}

static int cmd_gpio_uart(int argc, char **argv)
{
	char *p;
	if (argc != 2)
		return EC_ERROR_UNKNOWN;
	p = argv[1];
	while (*p)
		write_char(*(p++));

	msleep(1000);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(gu, cmd_gpio_uart, NULL, NULL, NULL);

static int cmd_dump(int argc, char **argv)
{
	int i, j;
	for (i = 0; i < 7; ++i) {
		ccprintf("Buf %d\n", i);
		ccprintf("   state = %d\n", fifo[i].state);
		ccprintf("   decoded = %d %c\n", fifo[i].decoded, fifo[i].decoded);
		ccprintf("   len = %d\n", fifo[i].len);
		ccprintf("   data = [");
		for (j = 0; j < fifo[i].len; ++j)
			ccprintf("%d ", fifo[i].data[j]);
		ccprintf("]\n");
	}
	ccprintf("head = %d, tail = %d\n", head, tail);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dump, cmd_dump, NULL, NULL, NULL);
