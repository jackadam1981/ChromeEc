/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Producer printing routines */

#include "producer_printing.h"

#include "printf.h"
#include "util.h"

struct vprintf_state {
	const struct printf_producer *producer;
	struct queue_chunk            chunk;
	size_t                        count;
};

static int addchar(void *context, int character)
{
	struct vprintf_state *state = (struct vprintf_state *) context;

	/*
	 * Newline to CarageReturn/Newline translation, this might not be
	 * the best place to do this.  Putting this here makes the producer
	 * printing API console specific.
	 */
	if (character == '\n' && addchar(context, '\r'))
		return 1;

	/*
	 * If we have exhausted our previous queue chunk advance the queue
	 * tail and allocate a new chunk.
	 */
	if (state->count == state->chunk.length) {
		const struct queue *queue = state->producer->producer.queue;

		queue_advance_tail(queue, state->count);

		state->count = 0;

		if (state->producer->next_chunk(state->producer, &state->chunk))
			return 1;
	}

	ASSERT(state->count < state->chunk.length);

	state->chunk.buffer[state->count] = character;
	state->count++;

	return 0;
}

/******************************************************************************
 * The next_chunk function to use for non-blocking printing.  This just returns
 * whatever chunk is currently available from the queue.
 */
int printf_producer_next_chunk_nonblocking(
	const struct printf_producer *producer,
	struct queue_chunk *chunk)
{
	*chunk = queue_get_write_chunk(producer->producer.queue);

	if (chunk->length == 0)
		return EC_ERROR_OVERFLOW;

	return EC_SUCCESS;
}

int printf_producer_vprintf(const struct printf_producer *producer,
			    const char *format,
			    va_list args)
{
	/*
	 * Initialize the state struct, this leaves the chunk and count zeroed.
	 * The first call to addchar will allocate the first write chunk from
	 * the queue.
	 */
	struct vprintf_state state = { .producer = producer };

	int rv = vfnprintf(addchar, &state, format, args);

	/*
	 * It is possible that we have arrived here with a zero count, the
	 * format string could have been empty.  In that case, do not advance
	 * the head by zero units.
	 */
	if (state.count)
		queue_advance_tail(producer->producer.queue, state.count);

	return rv;
}

int printf_producer_printf(const struct printf_producer *producer,
			   const char *format, ...)
{
	int     rv;
	va_list args;

	va_start(args, format);
	rv = printf_producer_vprintf(producer, format, args);
	va_end(args);

	return rv;
}

int printf_producer_puts(const struct printf_producer *producer,
			 const char *string)
{
	struct vprintf_state state = { .producer = producer };

	while (*string)
		if (addchar(&state, *string++))
			break;

	if (state.count)
		queue_advance_tail(producer->producer.queue, state.count);

	return *string ? EC_ERROR_OVERFLOW : EC_SUCCESS;
}

int printf_producer_putc(const struct printf_producer *producer,
			 int c)
{
	struct vprintf_state state = { .producer = producer };

	if (addchar(&state, c))
		return EC_ERROR_OVERFLOW;

	queue_advance_tail(producer->producer.queue, state.count);

	return EC_SUCCESS;
}
