/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Producer printing routines
 */

#ifndef __CROS_EC_PRODUCER_PRINTING_H
#define __CROS_EC_PRODUCER_PRINTING_H

#include "producer.h"
#include "task.h"

#include <stdarg.h>

/*
 * The printf_producer is a queue producer that provides printf, puts and putc
 * style methods.  These methods, declared below, eventually write into the
 * queue associated with this producer.
 */
struct printf_producer {
	struct producer producer;

	/*
	 * The next_chunk function is used by the printf_producer code when it
	 * needs to fetch a new write chunk from the producer queue.  This
	 * flexibility allows for the blocking version that blocks in this
	 * callback until a chunk is available (or a timeout occurs).
	 */
	int (*next_chunk)(const struct printf_producer *producer,
			  struct queue_chunk *chunk);
};

#define PRINTF_PRODUCER(QUEUE)						\
	((struct printf_producer) {					\
		.producer = {						\
			.queue = &QUEUE,				\
			.ops   = &((struct producer_ops const){		\
				.read = NULL,				\
			}),						\
		},							\
		.next_chunk = printf_producer_next_chunk_nonblocking,	\
	 })

/*
 * Non-blocking version of the next_chunk function.  This is used to initialize
 * the next_chunk function pointer in the printf_producer struct.
 */
int printf_producer_next_chunk_nonblocking(
	const struct printf_producer *producer,
	struct queue_chunk *chunk);

/*
 * Printf style output formatting producer.
 *
 * The producer_vprintf and producer_printf routines format a string and write
 * it out to the given producer using the format string and either a va_list
 * or a variable number of function arguments.
 *
 * The return value is EC_SUCCESS (zero) on success and EC_ERROR_OVERFLOW
 * (non-zero) on failure.  This is consistent with the other printf routines
 * in the EC codebase but not with the C specification of the similarly named
 * functions.
 */
int printf_producer_vprintf(const struct printf_producer *producer,
			    const char *format,
			    va_list args);

int printf_producer_printf(const struct printf_producer *producer,
			   const char *format, ...);

int printf_producer_puts(const struct printf_producer *producer,
			 const char *string);

int printf_producer_putc(const struct printf_producer *producer,
			 int c);

#endif  /* __CROS_EC_PRODUCER_PRINTING_H */
