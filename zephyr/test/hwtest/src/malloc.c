/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "shared_mem.h"
#include "stdlib.h"

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <malloc.h>

ZTEST_SUITE(malloc, NULL, NULL, NULL, NULL, NULL);

struct malloc_data {
	uint32_t size;
	uint8_t val;
	uint8_t *data;
};

static struct malloc_data test_data[] = {
	{ .size = 15, .val = 1, .data = NULL },
	{ .size = 1024, .val = 2, .data = NULL },
	{ .size = 86096, .val = 3, .data = NULL }
};

ZTEST(malloc, test_free_null)
{
	free(NULL);
}

ZTEST(malloc, test_malloc_different_sizes)
{
	/* Trim to make sure that previous tests haven't fragmented the heap. */
	malloc_trim(0);

	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		uint8_t *volatile ptr = malloc(test_data[i].size);
		zassert_not_equal(ptr, NULL, "%p");
		test_data[i].data = ptr;
		for (int j = 0; j < test_data[i].size; j++) {
			ptr[j] = test_data[i].val;
		}
	}

	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		uint8_t *ptr = test_data[i].data;
		/* Using zassert_equal results in too much logging. */
		for (int j = 0; j < test_data[i].size; j++) {
			if (ptr[j] != test_data[i].val) {
				zassert_unreachable();
			}
		}
	}

	for (int i = 0; i < ARRAY_SIZE(test_data); i++) {
		free(test_data[i].data);
	}
}

ZTEST(malloc, test_malloc_large)
{
	/* Trim to make sure that previous tests haven't fragmented the heap. */
	malloc_trim(0);
	uint8_t *volatile ptr = malloc(shared_mem_size() * 0.8);
	zassert_not_equal(ptr, NULL, "%p");
	free(ptr);
}

ZTEST(malloc, test_malloc_too_large)
{
	/* Trim to make sure that previous tests haven't fragmented the heap. */
	malloc_trim(0);
	uint8_t *volatile ptr = malloc(shared_mem_size() + 1);
	zassert_equal(ptr, NULL, "%p");
	free(ptr);
}
