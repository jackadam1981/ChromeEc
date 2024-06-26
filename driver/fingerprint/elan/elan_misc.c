/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* ELAN Platform Abstraction Layer */

#include "clock.h"
#include "elan_misc.h"
#include "fpsensor/fpsensor_console.h"
#include "shared_mem.h"
#include "timer.h"
#include "uart.h"

__staticlib_hook void *elan_malloc(uint32_t size)
{
	char *data;
	int rc;

	rc = shared_mem_acquire(size, &data);

	if (rc == EC_SUCCESS)
		return data;
	else {
		CPRINTS("Error - %s of size %u failed.", __func__, size);
		return NULL;
	}
}

__staticlib_hook void elan_free(void *data)
{
	shared_mem_release(data);
}

__staticlib_hook void elan_log_var(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	uart_vprintf(format, args);
	va_end(args);
}

__staticlib_hook void elan_clock_enable_module(enum module_id module,
					       int enable)
{
	clock_enable_module(module, enable);
}

__staticlib_hook timestamp_t elan_get_time(void)
{
	return get_time();
}
