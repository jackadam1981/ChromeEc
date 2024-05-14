/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "uart.h"

int cprints(enum console_channel channel, const char* format, ...)
{
	ARG_UNUSED(channel);
	ARG_UNUSED(format);
	return 0;
}

int cputs(enum console_channel channel, const char *outstr)
{
	ARG_UNUSED(channel);
	ARG_UNUSED(outstr);
	return 0;
}

int cprintf(enum console_channel channel, const char *format, ...)
{
	ARG_UNUSED(channel);
	ARG_UNUSED(format);
	return 0;
}

void uart_flush_output(void) {}
