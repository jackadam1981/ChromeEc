/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Producer printing routines, blocking version */

#include "producer_printing_blocking.h"

#include "printf.h"
#include "util.h"

/*
 * The next_chunk function to use for blocking printing.  This routing will wait
 * for a non-zero write chunk.
 */
int printf_producer_next_chunk_blocking(
	const struct printf_producer *producer,
	struct queue_chunk *chunk)
{
	struct printf_producer_blocking const *ppb =
		DOWNCAST(producer,
			 struct printf_producer_blocking const,
			 printf_producer);

	struct queue_chunk new_chunk =
		queue_get_write_chunk(producer->producer.queue);

	/*
	 * Loop waiting for the queue to have some free space.  This is signaled
	 * to our task by the TASK_EVENT_QUEUE event that is posted in the
	 * producers read callback.
	 */
	while (new_chunk.length == 0) {
		/*
		 * If we timeout while waiting for the other end to read from
		 * the queue then we break and return the chunk we've got,
		 * which will have zero length.
		 */
// TODO: Manually track deadline because this loop may execute more than once.
		if (task_wait_event_mask(TASK_EVENT_QUEUE, ppb->timeout) &
		    TASK_EVENT_TIMER)
			return EC_ERROR_TIMEOUT;

		new_chunk = queue_get_write_chunk(producer->producer.queue);
	}

	*chunk = new_chunk;

	return EC_SUCCESS;
}

static void printf_producer_read(struct producer const *producer, size_t count)
{
	/*
	 * Cast our way down from a generic producer, to a printf_producer,
	 * and finally down to the blocking version of printf_producer.
	 */
	struct printf_producer const *pp =
		DOWNCAST(producer,
			 struct printf_producer const,
			 producer);

	struct printf_producer_blocking const *ppb =
		DOWNCAST(pp,
			 struct printf_producer_blocking const,
			 printf_producer);

	/*
	 * Wake up the blocked task.  The task may not actually be blocked.
	 * in which case its first blocking wait will immediately return.  This
	 * could result in a zero length chunk being retrieved.  Thus the
	 * blocking next_chunk function loops until a non-zero chunk length
	 * is returned.
	 */
	task_set_event(ppb->task, TASK_EVENT_QUEUE, 0);
}

struct producer_ops const printf_blocking_producer_ops = {
	.read = printf_producer_read
};
