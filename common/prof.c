/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "util.h"
#include "console.h"

static struct entry {
	size_t count;
	void *func;
} entries[10];

#if 0
#define NO_CRASH
#endif

#if 0
#define ALT
#endif

__attribute__((no_instrument_function))
void _mcount(void)
{
	int i;
	__attribute__((unused)) void *func = __builtin_return_address(0);
	__attribute__((unused)) uint32_t ret;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
		if (!entries[i].func) {
			asm volatile ("nop");
#ifndef NO_CRASH
#ifndef ALT
			entries[i].func = func;
#else
			asm volatile ("mv %0, ra" : "=r"(ret));
			entries[i].func = (void *)ret;
#endif
#endif
			asm volatile ("nop");
			entries[i].count++;

			break;
		}

		if (entries[i].func == func) {
			entries[i].count++;
			break;
		}
	}
}

int y(int argc, char **argv)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(entries); ++i) {
#if 0
		if (!entries[i].func)
			break;
#endif

		ccprintf("%x %d\n", (unsigned int)entries[i].func, entries[i].count);
		cflush();
	}

	return 0;
}
DECLARE_SAFE_CONSOLE_COMMAND(y, y, NULL, "prof");
