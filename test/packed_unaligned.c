/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

#include "console.h"
#include "common.h"
#include "test_util.h"

#define CPRINTF(format, args...) cprintf(CC_COMMAND, format, ## args)

#pragma GCC push_options
#pragma GCC optimize ("O0")

/* Temporary buffer, to avoid using too much stack space. */
static uint8_t tmp[512];
/* This layer of indirection is actually neccesary to throw off the compiler */
// static void *ptr = &(tmp[1]);

typedef struct
__attribute__((packed))
{
	uint8_t a;
	uint32_t b;
} packed_struct_t;

typedef struct
{
	uint8_t a;
	uint32_t b;
} nopacked_struct_t;

/*
 * Check access to an unaligned and packed struct fields directly.
 */
static int test_packed_unaligned_access(void)
{
	volatile packed_struct_t *s = (packed_struct_t *)&(tmp[1]);

	CPRINTF("\n");
	CPRINTF("# Writing unaligned\n");
	CPRINTF("&s->a = 0x%pP\n", &s->a);
	CPRINTF("&s->b = 0x%pP\n", &s->b);
	cflush();
	s->a = 'h';
	s->b = 0x1BADD00D;

	CPRINTF("s->a = '%c'\n", s->a);
	CPRINTF("s->b = 0x%x\n", s->b);
	cflush();

	return EC_SUCCESS;
}

/*
 * Check access to an unaligned and packed struct fields directly.
 */
static int test_nopacked_unaligned_access(void)
{
	volatile nopacked_struct_t *s = (nopacked_struct_t *)&(tmp[1]);

	CPRINTF("\n");
	CPRINTF("# Writing unaligned\n");
	CPRINTF("&s->a = 0x%pP\n", &s->a);
	CPRINTF("&s->b = 0x%pP\n", &s->b);
	cflush();
	s->a = 'h';
	s->b = 0x1BADD00D;

	CPRINTF("s->a = '%c'\n", s->a);
	CPRINTF("s->b = 0x%x\n", s->b);
	cflush();

	return EC_SUCCESS;
}

void run_test(void)
{
	RUN_TEST(test_packed_unaligned_access);
	RUN_TEST(test_nopacked_unaligned_access);

	test_print_result();
}

#pragma GCC pop_options
