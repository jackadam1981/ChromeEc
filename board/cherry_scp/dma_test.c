/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is code that will live in DRAM only.
 */

#include "cache.h"
#include "common.h"
#include "console.h"
#include "cpu.h"
#include "link_defs.h"
#include "timer.h"
#include "util.h"
#include "memmap.h"

__SECTION(dram.bss) static int8_t bss_array[4];

void dump(void)
{
	int i;
	ccprintf("dump:\n");
	for (i = 0; i < sizeof(bss_array); i++)
		ccprintf("%x ", bss_array[i]);

	ccprintf("\n");
}

int command_dma_test(int argc, char **argv)
{
	ccprintf("bss %x\n", (unsigned int)bss_array);

	dump();
	dma_memset((uintptr_t)bss_array, 0x33, sizeof(bss_array));
	dump();

	cflush();

	return EC_SUCCESS;
}

DECLARE_SAFE_CONSOLE_COMMAND(dmatest, command_dma_test,
			     NULL, "dma test");
