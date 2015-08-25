/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Consumer printing routines
 */

#ifndef __CROS_EC_CONSUMER_PRINTING_H
#define __CROS_EC_CONSUMER_PRINTING_H

#include "consumer.h"

#include <stdarg.h>

/*
 * Printf style output formatting to a consumer with optional flushing.
 *
 * The consumer_vprintf and consumer_printf routines format a string and write
 * it out to the given consumer using the format string and either a va_list
 * or a variable number of function arguments.
 *
 * If flush is true then the consumers flush mechanism will be called after
 * each block is written to ensure that there is always space in the queue.
 *
 * If flush is false and the queue becomes full before the format string is
 * exhausted a non-zero value will be returned to indicate failure.
 *
 * The return value is zero on success and non-zero on failure.  This is
 * consistent with the other printf routines in the EC codebase but not with
 * the C specification of the similarly named functions.
 */
int consumer_vprintf(const struct consumer *consumer,
		     int flush,
		     const char *format,
		     va_list args);

int consumer_printf(const struct consumer *consumer,
		    int flush,
		    const char *format, ...);

int consumer_puts(const struct consumer *consumer,
		  int flush,
		  const char *string);

int consumer_putc(const struct consumer *consumer, int c);

#endif  /* __CROS_EC_CONSUMER_PRINTING_H */
