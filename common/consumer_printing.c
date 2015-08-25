/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Consumer printing routines */

#include "consumer_printing.h"

#include "printf.h"
#include "util.h"

struct consumer_vprintf_state {
	const struct consumer *consumer;
	int flush;
	struct queue_chunk chunk;
	size_t index;
};

static int consumer_addchar(void *context, int character)
{
	struct consumer_vprintf_state *state =
		(struct consumer_vprintf_state *) context;

	if (state->index == state->chunk.length) {
		queue_advance_tail(state->consumer->queue, state->index);

		if (state->flush)
			consumer_flush(state->consumer);

		state->chunk = queue_get_write_chunk(state->consumer->queue);
		state->index = 0;

		if (state->chunk.length == 0)
			return 1;
	}

	state->chunk.buffer[state->index] = character;
	state->index++;

	return 0;
}

int consumer_vprintf(const struct consumer *consumer,
		  int flush,
		  const char *format,
		  va_list args)
{
	struct consumer_vprintf_state state = {
		.consumer = consumer,
		.flush = flush,
		.chunk = queue_get_write_chunk(consumer->queue),
		.index = 0,
	};

	int rv = vfnprintf(consumer_addchar, &state, format, args);

	if (state.index) {
		queue_advance_tail(consumer->queue, state.index);

		if (state.flush)
			consumer_flush(consumer);
	}

	return rv;
}

int consumer_printf(const struct consumer *consumer,
		    int flush,
		    const char *format, ...)
{
	int rv;
	va_list args;

	va_start(args, format);
	rv = consumer_vprintf(consumer, flush, format, args);
	va_end(args);

	return rv;
}

int consumer_puts(const struct consumer *consumer,
		  int flush,
		  const char *string)
{
	size_t length = strlen(string);

	return (queue_add_units(consumer->queue, string, length) == length) ?
		EC_SUCCESS :
		EC_ERROR_OVERFLOW;
}

int consumer_putc(const struct consumer *consumer, int c)
{
	uint8_t byte = c;

	return (queue_add_unit(consumer->queue, &byte) == 1) ?
		EC_SUCCESS :
		EC_ERROR_OVERFLOW;
}
