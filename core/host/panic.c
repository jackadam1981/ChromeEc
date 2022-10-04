/* Copyright 2013 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "stack_trace.h"

struct host_panic_data {
	uint32_t reason;
	uint32_t info;
	uint8_t exception;
};

static struct host_panic_data panic_data;

void panic_assert_fail(const char *msg, const char *func, const char *fname,
		       int linenum)
{
	fprintf(stderr, "ASSERTION FAIL: %s:%d:%s - %s\n", fname, linenum, func,
		msg);
	task_dump_trace();

	puts("Fail!"); /* Inform test runner */
	fflush(stdout);

	exit(1);
}

void panic_set_reason(uint32_t reason, uint32_t info, uint8_t exception)
{
	panic_data.reason = reason;
	panic_data.info = info;
	panic_data.exception = exception;
}

void panic_get_reason(uint32_t *reason, uint32_t *info, uint8_t *exception)
{
	if (reason)
		*reason = panic_data.reason;
	if (info)
		*info = panic_data.info;
	if (exception)
		*exception = panic_data.exception;
}
