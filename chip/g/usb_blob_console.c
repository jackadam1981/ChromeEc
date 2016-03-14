/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "blob.h"
#include "console.h"
#include "printf.h"
#include "queue.h"
#include "task.h"
#include "usb_descriptor.h"
#include "util.h"

static int is_enabled = 1;
static int is_readonly;

struct tx_q_state {
	struct queue_chunk chunk;
	size_t index;
};
static struct queue const rx_q = QUEUE_NULL(USB_MAX_PACKET_SIZE, uint8_t);
static struct queue const tx_q = QUEUE_NULL(1024, uint8_t);

static void blob_has_output(void)
{
	size_t count;
	struct queue_chunk chunk = queue_get_read_chunk(&tx_q);

	if (chunk.length) {
		count = blob_send_bytes(chunk.buffer, chunk.length);
		queue_advance_head(&tx_q, count);
	}
}

static int __tx_char(void *context, int c)
{
	struct tx_q_state *state =
			(struct tx_q_state *) context;

	if (state->index == state->chunk.length) {
		queue_advance_tail(&tx_q, state->index);
		state->chunk = queue_get_write_chunk(&tx_q);
		state->index = 0;
		if (state->chunk.length == 0)
			return 1;
	}
	if (c == '\n' && __tx_char(state, '\r'))
		return 1;

	state->chunk.buffer[state->index] = c;
	state->index++;
	return 0;
}

/*
 * Public USB console implementation below.
 */
int usb_getc(void)
{
	int c;

	if (!is_enabled)
		return -1;

	if (QUEUE_REMOVE_UNITS(&rx_q, &c, 1))
		return c;
	return -1;
}

int usb_putc(int c)
{
	int ret = QUEUE_ADD_UNITS(&tx_q, &c, 1);

	if (ret)
		blob_has_output();
	return ret ? EC_SUCCESS : EC_ERROR_OVERFLOW;
}

int usb_puts(const char *outstr)
{
	int ret;
	struct tx_q_state state = {
		.chunk = queue_get_write_chunk(&tx_q),
		.index = 0,
	};
	if (is_readonly)
		return EC_SUCCESS;

	while (*outstr) {
		ret = __tx_char(&state, *outstr++);
		if (ret)
			break;
	}
	queue_advance_tail(&tx_q, state.index);
	blob_has_output();
	return *outstr ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}


int usb_vprintf(const char *format, va_list args)
{
	int ret;
	struct tx_q_state state = {
		.chunk = queue_get_write_chunk(&tx_q),
		.index = 0,
	};

	if (is_readonly)
		return EC_SUCCESS;

	ret = vfnprintf(__tx_char, &state, format, args);
	queue_advance_tail(&tx_q, state.index);
	blob_has_output();
	return ret;
}

void usb_console_enable(int enabled, int readonly)
{
	is_enabled = enabled;
	is_readonly = readonly;
}

void blob_sent_output(void)
{
	blob_has_output();
}

void blob_has_input(void)
{
	struct queue_chunk chunk = queue_get_write_chunk(&rx_q);
	int count = blob_get_bytes(chunk.buffer, chunk.length);

	queue_advance_tail(&rx_q, count);
	if (count)
		task_wake(TASK_ID_CONSOLE);
}
