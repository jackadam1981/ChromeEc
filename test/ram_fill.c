/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#include <stdint.h>

#include "console.h"
#include "common.h"
#include "link_defs.h"
#include "test_util.h"
#include "util.h"
#include "ram_table.h"

#define SPECIAL_VALUE 0x1BADD00D

#define ALIGN(addr,byte) ( (((uintptr_t)(addr)) + ((byte)-1))  & ~((byte)-1))


// static void fill_entry(const struct mem_range *e) {
// 	int *word;
// 	for (word = (int *)e->start; word < (int *)e->end; word++) {
// 		*word = SPECIAL_VALUE;
// 	}
// }

static int command_fill_special(int argc, char **argv)
{
	// const struct mem_range *e = &ram_table[0];
	// const struct mem_range *e_end = &ram_table[0] + (ram_table_size / sizeof(ram_table[0]));

	// for (; e < e_end; e++) {
	// 	ccprintf("Filling entry %pP to %pP\n", e->start, e->end);
	// 	fill_entry(e);
	// }

	int *word = (int *)ALIGN(__shared_mem_buf, sizeof(int));
	int *word_end = (int *)ALIGN(CONFIG_RAM_BASE + CONFIG_RAM_SIZE, sizeof(int));

	ccprintf("Filling entry %pP to %pP\n", word, word_end);

	for (; word < word_end; word++) {
		*word = SPECIAL_VALUE;
	}

	ccprintf("Filling complete.\n");
	ccprintf("Now reset (using pin or power-off/on) the EC and run runtest command.\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ram_fill, command_fill_special, NULL,
			"Fill all memory with special value for ram_fill test");


static int check_entry(const struct mem_range *e) {
	int *word;
	for (word = (int *)e->start; word < (int *)e->end; word++) {
		TEST_NE(*word, SPECIAL_VALUE, "0x%X");
	}
	return EC_SUCCESS;
}

static int test_check_for_reminants(void)
{
	const struct mem_range *e = &ram_table[0];
	const struct mem_range *e_end = &ram_table[0] + (ram_table_size / sizeof(ram_table[0]));

	ccprintf("\n");
	for (; e < e_end; e++) {
		ccprintf("Checking entry %pP to %pP\n", e->start, e->end);
		TEST_ASSERT(check_entry(e) == EC_SUCCESS);
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_check_for_reminants);

	test_print_result();
}
