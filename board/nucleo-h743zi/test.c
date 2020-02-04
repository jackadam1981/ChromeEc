/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>

#include "link_defs.h"
#include "common.h"
#include "console.h"

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

// Bit Error Rate
struct ber {
	size_t bit_errors;
	size_t bit_total;
};

static struct ber pattern_read_ber(int *values, size_t count) {
	size_t i;
	struct ber ber = {
		.bit_errors = 0,
		.bit_total = count * sizeof(int) * 8,
	};

	ccprintf("# Reading %zu values from 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		int v = values[i];
		int expected = (int)i;
		int b;
		for (b = 0; b < (sizeof(int) * 8); b++) {
			if ((v&BIT(b)) != (expected&BIT(b))) {
				ber.bit_errors++;
			}
		}
	}

	ccprintf("%zu / %zu BER\n", ber.bit_errors, ber.bit_total);
	cflush();

	return ber;
}

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

	ccprintf("# Writing %zu values to 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		values[i] = (int)i;
	}
}

static void zero(int *values, size_t count) {
	size_t i;

	ccprintf("# Writing %zu zeros to 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		values[i] = 0;
	}
}

static struct ber zero_read_ber(int *values, size_t count) {
	size_t i;
	struct ber ber = {
		.bit_errors = 0,
		.bit_total = count * sizeof(int) * 8,
	};

	ccprintf("# Reading %zu values from 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		int v = values[i];
		int expected = 0;
		int b;
		for (b = 0; b < (sizeof(int) * 8); b++) {
			if ((v&BIT(b)) != (expected&BIT(b))) {
				ber.bit_errors++;
			}
		}
	}

	ccprintf("%zu / %zu BER\n", ber.bit_errors, ber.bit_total);
	cflush();

	return ber;
}

/****************************************************************************/

static int command_write_rstdata(int argc, char **argv)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
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
		ccprintf("# Region: %s\n", regions[i].name);
		pattern_read(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(read, command_read_rstdata,
			     NULL, "Read reset data");

static int command_read_ber_rstdata(int argc, char **argv)
{
	int i;
	struct ber ber[ARRAY_SIZE(regions)];

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		ber[i] = pattern_read_ber(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("%zu\n", ber[i].bit_errors);
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(read_ber, command_read_ber_rstdata,
			     NULL, "Read BER of reset data");

static int command_zero_rstdata(int argc, char **argv)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		zero(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(zero, command_zero_rstdata,
			     NULL, "Zero out reset data");

static int command_zero_ber_rstdata(int argc, char **argv)
{
	int i;
	struct ber ber[ARRAY_SIZE(regions)];

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		ber[i] = zero_read_ber(regions[i].values, regions[i].count);
		ccprintf("\n");
		cflush();
	}

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("%zu\n", ber[i].bit_errors);
		cflush();
	}

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(zero_ber, command_zero_ber_rstdata,
			     NULL, "Read zero BER reset data");