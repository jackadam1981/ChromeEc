/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared memory module for Chrome EC */

#ifdef DESKTOP_MODE
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define EC_SUCCESS 0
#define EC_ERROR_BUSY 1
#define EC_ERROR_INVAL 2
static char __shared_mem_buf[17933];
#define ccprintf printf
#define DECLARE_HOOK(...)
#define mutex_lock(x)
#define mutex_unlock(x)
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
static int in_interrupt_context(void)
{
	return 0;
}
static uintptr_t system_usable_ram_end(void)
{
	return (uintptr_t)(__shared_mem_buf + sizeof(__shared_mem_buf) - 1);
}

/* Test always enabled in desktop mode. */
#ifndef TEST_BUILD
#define TEST_BUILD
#endif

#else  /* DESKTOP_MODE  yes ^^^^^^^^^^^^^ no vvvvvvvvvv */

#include <stddef.h>
#include <stdint.h>

#include "common.h"
#include "hooks.h"
#include "link_defs.h"
#include "system.h"
#include "task.h"
#include "util.h"

static struct mutex shmem_lock;

#endif /* DESKTOP_MODE no  ^^^^^^^^^^^^^ */

#ifdef TEST_BUILD
static int shmem_is_ok(int line);
static uint32_t test_map;
static void set_map_bit(uint32_t mask)
{
	test_map |= mask;
}
#else
#define set_map_bit(x)
#endif

/* Allow up to so many buffers to be allocated at once. */
#define MAX_ALLOCATIONS 10

/*
 * Structure to keep track of allocated buffers, save pointer and allocated
 * size to use when the buffer is freed.
 */
struct allocated_buf {
	void *buf_ptr;
	size_t buf_size;
};

/*
 * Array of currently allocated buffers, buf_ptr field set to NULL means the
 * element of the array is available to keep track of another allocation.
 */
static struct allocated_buf allocated_buffers[MAX_ALLOCATIONS];

/*
 * This structure is allocated at the base of the free memory chunk. This
 * starts with all memory belonging to one free chunk and then gets
 * fragmented/defragmented based on actuall allocations/releases.
 */
struct free_buffer {
	struct free_buffer *next_free_buffer;
	struct free_buffer *prev_free_buffer;
	size_t buffer_size;
};

struct free_buffer *free_buf_chain;

static void shared_mem_init(void)
{
	/*
	 * Use all the RAM we can.  The shared memory buffer is the last thing
	 * allocated from the start of RAM, so we can use everything up to the
	 * jump data at the end of RAM.
	 */
	free_buf_chain = (struct free_buffer *)__shared_mem_buf;
	free_buf_chain->next_free_buffer = NULL;
	free_buf_chain->prev_free_buffer = NULL;
	free_buf_chain->buffer_size = system_usable_ram_end() -
		(uintptr_t)__shared_mem_buf;
}
DECLARE_HOOK(HOOK_INIT, shared_mem_init, HOOK_PRIO_FIRST);

/* Called with the mutex lock acquired. */
static void do_release(struct free_buffer *ptr)
{
	int slot;
	size_t released_size;
	struct free_buffer *pfb;
	struct free_buffer *top;

	/* Do we know about this buffer? */
	for (slot = 0; slot < ARRAY_SIZE(allocated_buffers); slot++)
		if (allocated_buffers[slot].buf_ptr == ptr) {
			break;
		}

	if (slot == ARRAY_SIZE(allocated_buffers))
		return; /* Mark error here! */

	/* Release this slot for future use. */
	allocated_buffers[slot].buf_ptr = NULL;

	/* This is how much was allocated for this buffer. */
	released_size = allocated_buffers[slot].buf_size;

	/* Let's bring the released buffer back into the fold. */
	if (!free_buf_chain) {
		/*
		 * All memory had been allocated - this buffer the only free
		 * space there is.
		 */
		set_map_bit(1 << 0);
		free_buf_chain = ptr;
		free_buf_chain->buffer_size = released_size;
		free_buf_chain->next_free_buffer = NULL;
		free_buf_chain->prev_free_buffer = NULL;
		return;
	}

	if (ptr < free_buf_chain) {
		/*
		 * Insert this buffer in the beginning of the chain, possibly
		 * merging it with the first buffer of the chain.
		 */
		pfb = (struct free_buffer *)((uintptr_t)ptr + released_size);
		if (pfb == free_buf_chain) {
			set_map_bit(1 << 1);
			/* Merge the two buffers. */
			ptr->buffer_size = free_buf_chain->buffer_size +
				released_size;
			ptr->next_free_buffer =
				free_buf_chain->next_free_buffer;
		} else {
			set_map_bit(1 << 2);
			ptr->buffer_size = released_size;
			ptr->next_free_buffer = free_buf_chain;
			free_buf_chain->prev_free_buffer = ptr;
		}
		if (ptr->next_free_buffer) {
			set_map_bit(1 << 21);
			ptr->next_free_buffer->prev_free_buffer = ptr;
		} else {
			set_map_bit(1 << 22);
		}
		ptr->prev_free_buffer = NULL;
		free_buf_chain = ptr;
		return;
	}

	/*
	 * Need to merge the new free buffer into the existing chain. Find a
	 * spot for it, it should be above the highest address buffer which is
	 * still below the new one.
	 */
	pfb = free_buf_chain;
	while (pfb->next_free_buffer && (pfb->next_free_buffer < ptr))
		pfb = pfb->next_free_buffer;

	top = (struct free_buffer *)((uintptr_t)pfb + pfb->buffer_size);
	if (top == ptr) {
		/*
		 * The returned buffer is adjacent to an existing free buffer,
		 * below it, merge the two buffers.
		 */
		pfb->buffer_size += released_size;

		/*
		 * Is the returned buffer the exact gap between two free
		 * buffers?
		 */
		top = (struct free_buffer *)((uintptr_t)ptr + released_size);
		if (top == pfb->next_free_buffer) {
			/* Yes, it is. */
			set_map_bit(1 << 3);
			pfb->buffer_size += pfb->next_free_buffer->buffer_size;
			pfb->next_free_buffer =
				pfb->next_free_buffer->next_free_buffer;
			if (pfb->next_free_buffer) {
				set_map_bit(1 << 4);
				pfb->next_free_buffer->prev_free_buffer = pfb;
			} else {
				set_map_bit(1 << 23);
			}
		}
		return;
	}

	top = (struct free_buffer *)((uintptr_t)ptr + released_size);
	if (top == pfb->next_free_buffer) {
		/* The new buffer is adjacent with the one right above it. */
		set_map_bit(1 << 5);
		ptr->buffer_size = released_size + pfb->next_free_buffer->buffer_size;
		ptr->next_free_buffer = pfb->next_free_buffer->next_free_buffer;
	} else {
		/* Just include the new free buffer into the chain. */
		set_map_bit(1 << 6);
		ptr->next_free_buffer = pfb->next_free_buffer;
		ptr->buffer_size = released_size;
	}
	ptr->prev_free_buffer = pfb;
	pfb->next_free_buffer = ptr;
	if (ptr->next_free_buffer) {
		set_map_bit(1 << 7);
		ptr->next_free_buffer->prev_free_buffer = ptr;
	} else {
		set_map_bit(1 << 24);
	}
}

/* Called with the mutex lock acquired. */
static int do_acquire(int size, char **dest_ptr)
{
	int headroom = 0x10000000; /* we'll never have this much. */
	struct free_buffer *pfb;
	struct free_buffer *candidate = 0;
	int slot;

	/* Is there a slot for a new allocation? */
	for (slot = 0; slot < ARRAY_SIZE(allocated_buffers); slot++)
		if (!allocated_buffers[slot].buf_ptr)
			break;

	if (slot == ARRAY_SIZE(allocated_buffers)) {
		set_map_bit(1 << 8);
		return EC_ERROR_BUSY;
	}

	/* To keep things simple lets align the size. */
	if (size < (sizeof(struct free_buffer))) {
		set_map_bit(1 << 9);
		size = sizeof(struct free_buffer);
	} else {
		set_map_bit(1 << 10);
		size = (size + 3) & ~3;
	}

	pfb = free_buf_chain;
	while (pfb) {
		if ((pfb->buffer_size > size) &&
		    ((pfb->buffer_size - size) < headroom)) {
			/* this is a new candidate. */
			headroom = pfb->buffer_size - size;
			candidate = pfb;
		}
		pfb = pfb->next_free_buffer;
	}

	if (!candidate) {
		set_map_bit(1 << 11);
		return EC_ERROR_BUSY;
	}

	/* The user will get something. */
	allocated_buffers[slot].buf_ptr = candidate;
	*dest_ptr = (char *)candidate;

	/* Now let's take the candidate out of the free buffer chain. */
	if (headroom <= sizeof(struct free_buffer)) {
		/*
		 * The entire buffer should be allocated, there is no need to
		 * re-define its tail as a new free buffer.
		 */
		allocated_buffers[slot].buf_size = candidate->buffer_size;
		if (candidate == free_buf_chain) {
			/*
			 * The next buffer becomes the head of the free buffer
			 * chain.
			 */
			free_buf_chain = candidate->next_free_buffer;
			if (free_buf_chain) {
				set_map_bit(1 << 12);
				free_buf_chain->prev_free_buffer = 0;
			} else {
				set_map_bit(1 << 13);
			}
		} else {
			candidate->prev_free_buffer->next_free_buffer =
				candidate->next_free_buffer;
			if (candidate->next_free_buffer) {
				set_map_bit(1 << 14);
				candidate->next_free_buffer->prev_free_buffer =
					candidate->prev_free_buffer;
			} else {
				set_map_bit(1 << 15);
			}
		}
		return EC_SUCCESS;
	}

	allocated_buffers[slot].buf_size = size;

	/* Candidate's tail becomes a new free buffer. */
	pfb = (struct free_buffer *)((uintptr_t)candidate + size);
	pfb->buffer_size = headroom;
	pfb->next_free_buffer = candidate->next_free_buffer;
	pfb->prev_free_buffer = candidate->prev_free_buffer;

	if (pfb->next_free_buffer) {
		set_map_bit(1 << 16);
		pfb->next_free_buffer->prev_free_buffer = pfb;
	} else {
		set_map_bit(1 << 17);
	}

	if (candidate == free_buf_chain) {
		set_map_bit(1 << 18);
		free_buf_chain = pfb;
	} else {
		set_map_bit(1 << 19);
		pfb->prev_free_buffer->next_free_buffer = pfb;
	}
	return EC_SUCCESS;
}

int shared_mem_size(void)
{
	struct free_buffer *pfb;
	size_t max_available = 0;

	mutex_lock(&shmem_lock);

	/* Find the maximum available buffer size. */
	pfb = free_buf_chain;
	while (pfb) {
		if (pfb->buffer_size > max_available)
			max_available = pfb->buffer_size;
		pfb = pfb->next_free_buffer;
	}

	mutex_unlock(&shmem_lock);
	return max_available;
}

int shared_mem_acquire(int size, char **dest_ptr)
{
	int rv;

	if (in_interrupt_context())
		return EC_ERROR_INVAL;  /* Mark error here! */

	if (!free_buf_chain)
		return EC_ERROR_BUSY;  /* Mark error here! */

	mutex_lock(&shmem_lock);
	rv = do_acquire(size, dest_ptr);
	mutex_unlock(&shmem_lock);

	return rv;
}

void shared_mem_release(void *ptr)
{
	if (in_interrupt_context())
		return;		/* Mark error here! */

	mutex_lock(&shmem_lock);
	do_release(ptr);
	mutex_unlock(&shmem_lock);
}


#ifdef TEST_BUILD

static int total_size;
static int counter = 100000000;
static uint32_t next = 127;
static uint32_t myrand(void)
{
	next = next * 1103515245 + 12345;
	return((uint32_t)(next/65536) % 32768);
}

static struct allocated_buf allocations[12];
static int check_for_overlaps(void)
{
	int i;
	int in_allocated;

	int allocations_count, allocated_count;

	allocations_count = allocated_count = 0;
	for (i = 0; i < ARRAY_SIZE(allocations); i++) {
		int j;

		if (!allocations[i].buf_ptr)
			continue;

		allocations_count++;
		in_allocated = 0;
		allocated_count = 0;
		for (j = 0; j < ARRAY_SIZE(allocated_buffers);
		     j++) {
			int allocated_size, allocation_size;

			if (!allocated_buffers[j].buf_ptr)
				continue;

			allocated_count++;
			if (allocations[i].buf_ptr !=
			    allocated_buffers[j].buf_ptr)
				continue;

			allocated_size = allocated_buffers[j].buf_size;
			allocation_size = allocations[i].buf_size;
			if ((allocation_size > allocated_size) ||
			    (((allocation_size + 2 * sizeof(struct free_buffer)) <
			      allocated_size))) {
				ccprintf("inconsistency: allocated %d(size %d)"
					 " allocation %d(size %d)\n",
					 j, allocated_size, i, allocation_size);
				return 0;
			}
			if (!in_allocated++)
				continue;
			ccprintf("inconsistency: duplicated match\n");
			return 0;
		}
		if (!in_allocated) {
			ccprintf("missing match %p!\n", allocations[i].buf_ptr);
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
	struct free_buffer *pbuf = free_buf_chain;

	if (pbuf && pbuf->prev_free_buffer) {
		ccprintf("Bad free buffer list start %p\n", pbuf);
		goto bailout;
	}

	while (pbuf && (count < 15)) {
		struct free_buffer *top;

		running_size += pbuf->buffer_size;
		count++;

		top = (struct free_buffer *)((uintptr_t)pbuf +
					     pbuf->buffer_size);
		if (pbuf->next_free_buffer && (top >= pbuf->next_free_buffer)) {
			ccprintf("%s:%d - inconsistent buffer size at %p\n",
				 __func__, __LINE__, pbuf);
			goto bailout;
		}
		if (pbuf->next_free_buffer &&
		    (pbuf->next_free_buffer->prev_free_buffer != pbuf)) {
			ccprintf("%s:%d - inconsistent next buffer at %p\n",
				 __func__, __LINE__, pbuf);
			goto bailout;
		}
		pbuf = pbuf->next_free_buffer;
	}

	if (count > 7)
		set_map_bit(1 << 20);
	if (pbuf) {
		ccprintf("Too many buffers in the chain\n");
		goto bailout;
	}

	for (count = 0; count < ARRAY_SIZE(allocated_buffers); count++)
		if (allocated_buffers[count].buf_ptr)
			running_size += allocated_buffers[count].buf_size;

	if (total_size == running_size) {
		if  (!check_for_overlaps())
			goto bailout;
		return 1;
	}

	if (!total_size) {
		total_size = running_size;
		if (!check_for_overlaps())
			goto bailout;
		return 1;
	}

 bailout:
	ccprintf("Line %d, counter %d. The list has been corrupted, "
		 "total size %d, running size %d\n",
		 line, counter, total_size, running_size);
	while(1)
		;
	return 0;
}

static void test_shmem(void)
{
	uint32_t r_data;
	int index;

        r_data = myrand();

	while (counter--) {
		char *shptr;

		if ((test_map & ((1 << 25) - 1)) == ((1 << 25) - 1)) {
			ccprintf("Done testing, counter at %d\n", counter);
			return;
		}
		index = r_data % ARRAY_SIZE(allocations);
		if (allocations[index].buf_ptr) {
			shared_mem_release(allocations[index].buf_ptr);
			allocations[index].buf_ptr = 0;
			if (!shmem_is_ok(__LINE__))
				return;
		} else {
			size_t alloc_size = r_data % (sizeof(__shared_mem_buf) / 2);

			if (shared_mem_acquire(alloc_size, &shptr) == EC_SUCCESS) {
				allocations[index].buf_ptr = shptr;
				allocations[index].buf_size = alloc_size;
				if (!shmem_is_ok(__LINE__))
					return;
			}
		}
		r_data = myrand();
	}

	for (index = 0; index < ARRAY_SIZE(allocations); index++)
		if (allocations[index].buf_ptr) {
			shared_mem_release(allocations[index].buf_ptr);
			allocations[index].buf_ptr = 0;
			if (!shmem_is_ok(__LINE__))
				return;
		}
}

#endif /* TEST_BUILD ^^^^^^^^^^ yes */

#ifdef DESKTOP_MODE
int main (int argc, char **argv)
{
	shared_mem_init();
	test_shmem();
	ccprintf("Test map is %x\n", test_map);

	return 0;
}
#endif
