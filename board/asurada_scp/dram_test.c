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

__SECTION(dram.bss) static int8_t bss_array[4];
__SECTION(dram.data) static int8_t data_array[4] = { 0xde, 0xad, 0xbe, 0xef };

__SECTION(dram.rodata) static const int8_t const_data_array[4] = { 5, 5, 6, 6 };

__SECTION(dram.bss) static int counter;

/* __SECTION(dram.text) */
static void print_array(const char *name, int8_t *array, size_t size)
{
	int i;

	ccprintf("%s: ", name);
	for (i = 0; i < size; ++i)
		ccprintf("%02x ", array[i] & 0xff);
	ccprintf("\n");
}

/* __SECTION(dram.text)  */
int command_dram_test(int argc, char **argv)
{
	ccprintf("self %x bss %x data %x const %x counter %x\n",
			(unsigned int)&command_dram_test,
			(unsigned int)bss_array,
			(unsigned int)data_array,
			(unsigned int)const_data_array,
			(unsigned int)&counter);
	cflush();
	msleep(100);

	ccprintf("original:\n");
	print_array("  bss_array", bss_array, ARRAY_SIZE(bss_array));
	print_array("  data_array", data_array, ARRAY_SIZE(data_array));

	ccprintf("copying data_array to bss_array:\n");
	memcpy(bss_array, data_array, sizeof(bss_array));
	print_array("  bss_array", bss_array, ARRAY_SIZE(bss_array));

	ccprintf("copying const_data_array to data_array:\n");
	memcpy(data_array, const_data_array, sizeof(data_array));
	print_array("  data_array", data_array, ARRAY_SIZE(data_array));

	ccprintf("counter: %d\n", counter++);

	cflush();

#define CASE 1
#if CASE == 1
	cache_writeback_dcache();
	cache_invalidate_dcache();
#elif CASE == 2
	cache_flush_dcache();
#elif CASE == 3
	cache_invalidate_dcache();
#endif

	return EC_SUCCESS;
}

DECLARE_SAFE_CONSOLE_COMMAND(dramtest, command_dram_test,
			     NULL, "DRAM placing test");
