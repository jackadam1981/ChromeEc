/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "malloc_pal.h"

#include "fpc_malloc_sequences.h"

#define CHUNK_SIZE (512*1024)
static uint8_t __heap[CHUNK_SIZE];

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define MAX(a, b) (((a) > (b))?(a):(b))

static void *alloc_fill(size_t size, uint8_t n)
{
	int rv;
	void *ptr;

	rv = FpcMalloc(&ptr, size);
	if (rv)
		return NULL;
	memset(ptr, n, size);
	return ptr;
}

static int free_check(uint8_t *ptr, size_t size, uint8_t n)
{
	int rv = 0;
	int i;

	for (i = 0; i < size; i++)
		if (ptr[i] != n)
			rv++;
	FpcFree(ptr);
	return rv;
}

static void testcase(struct step *steps, int count, void **ptrs)
{
	unsigned int i;
	size_t mem_max = 0, mem_left = 0;
	int chunks_max = 0;

	for (i = 0; i < count; i++) {
		chunks_max = MAX(chunks_max, steps[i].idx);
		mem_left += steps[i].size;
		mem_max = MAX(mem_max, mem_left);
		if (steps[i].size > 0) {
			ptrs[steps[i].idx] = alloc_fill(steps[i].size,
							steps[i].idx);
			if (!ptrs[steps[i].idx])
				printf("FAIL Malloc/%d: %d/%d\n", i,
					steps[i].idx, steps[i].size);
		} else {
			if (free_check(ptrs[steps[i].idx], -steps[i].size,
					steps[i].idx))
				printf("FAIL Free/%d: %d/%d\n", i,
					steps[i].idx, steps[i].size);
		}
	}
	if (mem_left)
		printf("FAIL remaining memory: %zu\n", mem_left);
	printf("%d steps -- max mem used: %zu -- max chunks: %d\n", count,
		mem_max, chunks_max + 1);
}

int main(int argc, char **argv)
{
	memset(ident_ptr, 0, sizeof(ident_ptr));
	FpcMallocInit(__heap, sizeof(__heap));
	testcase(ident_seq, ARRAY_SIZE(ident_seq), ident_ptr);

	memset(enrol_ptr, 0, sizeof(enrol_ptr));
	FpcMallocInit(__heap, sizeof(__heap));
	testcase(enrol_seq, ARRAY_SIZE(enrol_seq), enrol_ptr);

	return 0;
}
