/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Producer printing routines
 */

#ifndef __CROS_EC_PRODUCER_PRINTING_BLOCKING_H
#define __CROS_EC_PRODUCER_PRINTING_BLOCKING_H

#include "producer_printing.h"

/*
 * This is the blocking version of the printf_producer.  It extends the
 * printf_producer with a task and timeout.  The task should be the task that
 * will be calling the printf, puts and putc routines below.  The timeout is
 * the maximum time that a call will block waiting for a free chunk.  This can
 * be zero to indicate that calls should block indefinitely.
 */
struct printf_producer_blocking {
	struct printf_producer printf_producer;

	task_id_t task;
	int       timeout;
};

#define PRINTF_PRODUCER_BLOCKING(QUEUE, TASK, TIMEOUT)			\
	((struct printf_producer_blocking) {				\
		.printf_producer = {					\
			.producer = {					\
				.queue = &QUEUE,			\
				.ops   = &printf_blocking_producer_ops,	\
			},						\
			.next_chunk = printf_producer_next_chunk_blocking, \
		},							\
		.task    = TASK,					\
		.timeout = TIMEOUT,					\
	 })								\

/*
 * Blocking version of the next_chunk function.  This is used to initialize the
 * next_chunk function pointer in the printf_producer_blocking struct.
 */
int printf_producer_next_chunk_blocking(
	const struct printf_producer *producer,
	struct queue_chunk *chunk);

/*
 * Producer ops for printf_producer_blocking.  The producer_ops read function
 * is used to wake the possibly blocked task up when the consumer end of the
 * queue has read something, freeing space in the queue for the producer.
 */
extern struct producer_ops const printf_blocking_producer_ops;

#endif  /* __CROS_EC_PRODUCER_PRINTING_BLOCKING_H */
