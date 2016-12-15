/*
 * Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "link_defs.h"
#include "shared_mem.h"
#include "test_util.h"

static int total_size;
static int counter = 20000000;
static uint32_t next = 127;

static uint32_t myrand(void)
{
	next = next * 1103515245 + 12345;
	return((uint32_t)(next/65536) % 32768);
}

static struct {
	void *buf;
	size_t buffer_size;
} allocations[12];

static int check_for_overlaps(void)
{
	int i;
	int in_allocated;
	int allocations_count, allocated_count;

	allocations_count = allocated_count = 0;
	for (i = 0; i < ARRAY_SIZE(allocations); i++) {
		struct shm_buffer *allocced_buf;

		if (!allocations[i].buf)
			continue;

		allocations_count++;
		in_allocated = 0;
		allocated_count = 0;
		for (allocced_buf = allocced_buf_chain;
		     allocced_buf;
		     allocced_buf = allocced_buf->next_buffer) {
			int allocated_size, allocation_size;

			allocated_count++;
			if (allocations[i].buf != (allocced_buf + 1))
				continue;

			allocated_size = allocced_buf->buffer_size;
			allocation_size = allocations[i].buffer_size;
			if ((allocation_size > allocated_size) ||
			    ((allocated_size - allocation_size) >
			     (2 * sizeof(struct shm_buffer)))) {
				ccprintf("inconsistency: allocated (size %d)"
					 " allocation %d(size %d)\n",
					 allocated_size, i, allocation_size);
				return 0;
			}
			if (!in_allocated++)
				continue;
			ccprintf("inconsistency: duplicated match\n");
			return 0;
		}
		if (!in_allocated) {
			ccprintf("missing match %p!\n", allocations[i].buf);
			return 0;
		}
	}
	if (allocations_count != allocated_count) {
		ccprintf("count mismatch (%d != %d)!\n",
			 allocations_count, allocated_count);
		return 0;
	}
	return 1;
}

static int shmem_is_ok(int line)
{
	int count = 0;
	int running_size = 0;
	struct shm_buffer *pbuf = free_buf_chain;

	if (pbuf && pbuf->prev_buffer) {
		ccprintf("Bad free buffer list start %p\n", pbuf);
		goto bailout;
	}

	while (pbuf) {
		struct shm_buffer *top;

		running_size += pbuf->buffer_size;
		if (count++ > 100)
			goto bailout;  /* Is there a loop? */

		top = (struct shm_buffer *)((uintptr_t)pbuf +
					     pbuf->buffer_size);
		if (pbuf->next_buffer && (top >= pbuf->next_buffer)) {
			ccprintf("%s:%d - inconsistent buffer size at %p\n",
				 __func__, __LINE__, pbuf);
			goto bailout;
		}
		if (pbuf->next_buffer &&
		    (pbuf->next_buffer->prev_buffer != pbuf)) {
			ccprintf("%s:%d - inconsistent next buffer at %p\n",
				 __func__, __LINE__, pbuf);
			goto bailout;
		}
		pbuf = pbuf->next_buffer;
	}

	if (count > 7)
		set_map_bit(1 << 20);
	if (pbuf) {
		ccprintf("Too many buffers in the chain\n");
		goto bailout;
	}

	/* Add allocated sizes. */
	for (pbuf = allocced_buf_chain; pbuf; pbuf = pbuf->next_buffer)
		running_size += pbuf->buffer_size;

	if (total_size) {
		if (total_size != running_size)
			goto bailout;
	} else {
		total_size = running_size;
	}

	if (!check_for_overlaps())
		goto bailout;

	return 1;

 bailout:
	ccprintf("Line %d, counter %d. The list has been corrupted, "
		 "total size %d, running size %d\n",
		 line, counter, total_size, running_size);
	return 0;
}

static uint32_t test_map;

void run_test(void)
{
	uint32_t r_data;
	int index;
	const int shmem_size = shared_mem_size();

	r_data = myrand();

	while (counter--) {
		char *shptr;

		if (!(counter % 500000))
			ccprintf("%d\n", counter);

		if ((test_map & ALL_PATHS_MASK) == ALL_PATHS_MASK) {
			if (test_map & ~ALL_PATHS_MASK) {
				ccprintf("Unexpected mask bits set: %x"
					 ", counter %d\n",
					 test_map & ~ALL_PATHS_MASK,
					 counter);
				test_fail();
				return;
			}
			ccprintf("Done testing, counter at %d\n", counter);
			test_pass();
			return;
		}
		index = r_data % ARRAY_SIZE(allocations);
		if (allocations[index].buf) {
			shared_mem_release(allocations[index].buf);
			allocations[index].buf = 0;
			if (!shmem_is_ok(__LINE__)) {
				test_fail();
				return;
			}
		} else {
			size_t alloc_size = r_data % (shmem_size / 2);

			if (shared_mem_acquire(alloc_size, &shptr) ==
			    EC_SUCCESS) {
				allocations[index].buf = (void *) shptr;
				allocations[index].buffer_size = alloc_size;
				if (!shmem_is_ok(__LINE__)) {
					test_fail();
					return;
				}
			}
		}
		r_data = myrand();
	}

	for (index = 0; index < ARRAY_SIZE(allocations); index++)
		if (allocations[index].buf) {
			shared_mem_release(allocations[index].buf);
			allocations[index].buf = NULL;
			if (!shmem_is_ok(__LINE__)) {
				test_fail();
				return;
			}
		}

	ccprintf("Did not pass all paths, map %x != %x\n",
		 test_map, ALL_PATHS_MASK);
	test_fail();
}

void set_map_bit(uint32_t mask)
{
	test_map |= mask;
}

