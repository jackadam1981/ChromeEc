/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>

#include "link_defs.h"
#include "common.h"
#include "console.h"
// #include "gpio.h"

// #define FP_FRAME_SECTION    __SECTION(ahb4)
// #define FP_TEMPLATE_SECTION __SECTION(ahb)

static int ahb[(288/4) * 1024]  __SECTION(ahb);
static int ahb4[(32/4) * 1024]  __SECTION(ahb4);
static int backup[(4/4) * 1024] __SECTION(backup);
static int itcm[(64/4) * 1024]  __SECTION(itcm);
static int preserved_data[4096] __SECTION(preserve_data);
static int bkpreg[32]           __SECTION(bkpreg);

struct {
	char *name;
	int *values;
	size_t count;
} regions[] = {
	{"ahb",            ahb,            ARRAY_SIZE(ahb)},
	{"ahb4",           ahb4,           ARRAY_SIZE(ahb4)},
	{"backup",         backup,         ARRAY_SIZE(backup)},
	{"itcm",           itcm,           ARRAY_SIZE(itcm)},
	{"preserved_data", preserved_data, ARRAY_SIZE(preserved_data)},
	{"bkpreg",         bkpreg,         ARRAY_SIZE(bkpreg)}
};

static void pattern_read(int *values, size_t count) {
	size_t i;
	size_t matched = 0;

	ccprintf("# Reading %zu values from 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		int v = values[i];
		// ccprintf("i = %d: ", i);
		if (v == (int)i) {
			matched++;
			// ccprintf("%s\n", "Matched !");
		} else {
			// ccprintf("%s\n", "NoMatched");
			// ccprintf("Expected %d but found %d\n", i, v);
		}
		// cflush();
	}

	ccprintf("%zu / %zu matched\n", matched, count);
	cflush();
}

static void pattern_write(int *values, size_t count) {
	size_t i;

	ccprintf("# Writing %zu values to 0x%pP\n",count, values);
	cflush();

	for (i = 0; i < count; i++) {
		values[i] = (int)i;
	}
}

/****************************************************************************/

static int command_write_rstdata(int argc, char **argv)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# %s\n", regions[i].name);
		pattern_write(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(write, command_write_rstdata,
			     NULL, "Write reset data");

static int command_read_rstdata(int argc, char **argv)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# %s\n", regions[i].name);
		pattern_read(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(read, command_read_rstdata,
			     NULL, "Read reset data");