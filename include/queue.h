/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Queue data structure.
 */
#ifndef INCLUDE_QUEUE_H
#define INCLUDE_QUEUE_H

#include "common.h"

#include <stddef.h>
#include <stdint.h>

/* Generic queue container. */

/*
 * RAM state for a queue.
 */
typedef struct {
	/*
	 * The queue head and tail pointers are not wrapped until they are
	 * needed to access the queue buffer.  This has a number of advantages,
	 * the queue doesn't have to waste an entry to disambiguate full and
	 * empty for one.  It also provides a convenient total enqueue/dequeue
	 * log (one that does wrap at the limit of a size_t however).
	 *
	 * Empty:
	 *     head == tail
	 *
	 * Full:
	 *     head - tail == buffer_units
	 */
	size_t head; /* head: next to dequeqe */
        size_t tail; /* head: next to enqueqe */
} queue_state;

/*
 * Queue configuration stored in flash.
 */
typedef struct {
	queue_state volatile * state;

	size_t    buffer_units; /* size of buffer (in units) */
	size_t    unit_bytes;   /* size of unit   (in byte) */
	uint8_t * buffer;
} queue;

/*
 * Convenience macro for construction of a Queue along with its backing buffer
 * and state structure.
 */
#define QUEUE_CONFIG(NAME, SIZE, TYPE)					\
	static TYPE CONCAT2(NAME, _buffer)[SIZE];			\
									\
	static queue_state CONCAT2(NAME, _state);			\
	queue const NAME =						\
	{								\
		.state        = &CONCAT2(NAME, _state),			\
		.buffer_units = SIZE,					\
		.unit_bytes   = sizeof(TYPE),				\
		.buffer       = (uint8_t *) CONCAT2(NAME, _buffer),	\
	};

/* Initialize the queue to empty state. */
void queue_init(queue const * q);

/* Return TRUE if the queue is empty. */
int queue_is_empty(queue const * q);

/* Return the number of units stored in the queue. */
size_t queue_count(queue const * q);

/* Return the number of units worth of free space the queue has. */
size_t queue_space(queue const * q);

/* Add multiple units into queue. */
size_t queue_add_units(queue const * q, void const * src, size_t count);

/* Remove multiple units from the begin of the queue. */
size_t queue_remove_units(queue const * q, void * dest, size_t count);

/* Peek (return but don't remove) the i'th element in the queue. */
size_t queue_peek_units(queue const * q, void * dest, size_t i, size_t count);

#endif //INCLUDE_QUEUE_H
