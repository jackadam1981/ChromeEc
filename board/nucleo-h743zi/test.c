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

typedef int (*pattern_t)(int *values, size_t count, size_t index);

static int pattern_index(int *values, size_t count, size_t index) {
	return (int)index;
}

static int pattern_zero(int *values, size_t count, size_t index) {
	return 0;
}

static int pattern_ones(int *values, size_t count, size_t index) {
	return 1;
}

// Bit Error Rate
struct ber {
	size_t bit_errors;
	size_t bit_total;
};

static struct ber pattern_read_ber(int *values, size_t count, pattern_t pattern) {
	size_t i;
	struct ber ber = {
		.bit_errors = 0,
		.bit_total = count * sizeof(int) * 8,
	};

	ccprintf("# Reading %zu values from 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		int v = values[i];
		int expected = pattern(values, count, i);
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

static void pattern_read(int *values, size_t count, pattern_t pattern) {
	size_t i;
	size_t matched = 0;

	ccprintf("# Reading %zu values from 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		int v = values[i];
		// ccprintf("i = %d: ", i);
		if (v == pattern(values, count, i)) {
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

static void pattern_write(int *values, size_t count, pattern_t pattern) {
	size_t i;

	ccprintf("# Writing %zu values to 0x%pP\n", count, values);
	cflush();

	for (i = 0; i < count; i++) {
		values[i] = pattern(values, count, i);
	}
}

/****************************************************************************/

static void set(pattern_t pattern)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		pattern_write(regions[i].values, regions[i].count, pattern);
		ccprintf("\n");
		cflush();
	}
}

static void check(pattern_t pattern)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		pattern_read(regions[i].values, regions[i].count, pattern);
		ccprintf("\n");
		cflush();
	}
}

static void check_ber(pattern_t pattern)
{
	int i;
	struct ber ber[ARRAY_SIZE(regions)];

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("# Region: %s\n", regions[i].name);
		ber[i] = pattern_read_ber(regions[i].values, regions[i].count, pattern);
		ccprintf("\n");
		cflush();
	}

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		ccprintf("%zu\n", ber[i].bit_errors);
		cflush();
	}
}

/****************************************************************************/

static int command_write_rstdata(int argc, char **argv)
{
	set(pattern_index);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(write, command_write_rstdata,
			     NULL, "Write reset data");

static int command_read_rstdata(int argc, char **argv)
{
	check(pattern_index);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(read, command_read_rstdata,
			     NULL, "Read reset data");

static int command_read_ber_rstdata(int argc, char **argv)
{
	check_ber(pattern_index);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(read_ber, command_read_ber_rstdata,
			     NULL, "Read BER of reset data");

/**********************************************************************/

static int command_zero_rstdata(int argc, char **argv)
{
	set(pattern_zero);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(zero, command_zero_rstdata,
			     NULL, "Zero out reset data");

static int command_zero_ber_rstdata(int argc, char **argv)
{
	check_ber(pattern_zero);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(zero_ber, command_zero_ber_rstdata,
			     NULL, "Read zero BER reset data");

/**********************************************************************/

static int command_ones_rstdata(int argc, char **argv)
{
	set(pattern_ones);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ones, command_ones_rstdata,
			     NULL, "Ones out reset data");

static int command_ones_ber_rstdata(int argc, char **argv)
{
	check_ber(pattern_ones);

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ones_ber, command_ones_ber_rstdata,
			     NULL, "Read ones BER reset data");