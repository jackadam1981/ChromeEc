/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* FPC Platform Abstraction Layer */

#include "shared_mem.h"
#include "uart.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

__staticlib_hook void *fpc_malloc(uint32_t size)
{
	char *data;
	int rc;

	printf("FPC Alloc %d\n", size);

	rc = shared_mem_acquire(size, (char **)&data);

	if (rc == 0)
		return data;
	else {
		printf("FPC Alloc Failed.\n");
		return NULL;
	}
}

__staticlib_hook void fpc_free(void *data)
{
	shared_mem_release(data);
}

/* Not in release */
__staticlib_hook void fpc_assert_fail(const char *file, uint32_t line,
				      const char *func, const char *expr)
{
}

__staticlib_hook void fpc_log_var(const char *source, uint8_t level,
				  const char *format, ...)
{
	va_list args;

	va_start(args, format);
	uart_vprintf(format, args);
	va_end(args);
}
