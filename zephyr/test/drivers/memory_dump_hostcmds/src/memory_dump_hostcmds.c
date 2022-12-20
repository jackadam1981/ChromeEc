/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "memory_dump_hostcmds.h"
#include "test/drivers/test_state.h"

#include <stdlib.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>
#include <zephyr/ztest_assert.h>

/* Simple local structs for storing a memory dump */
struct mem_dump {
	uint16_t count;
	struct mem_segment *segments;
};
struct mem_segment {
	uint32_t addr;
	uint32_t size;
	uint8_t *mem;
};

static void before(void *data)
{
	clear_memory_dump();
}

K_THREAD_STACK_DEFINE(test_stack, 256);

static void test_thread_entry(void *a, void *b, void *c)
{
	/* Just sleep for awhile */
	k_msleep(2 * 1000);
}

/* Check if a buffer contains a given value */
bool buffer_contains(const void *buffer, size_t buffer_size, const void *value,
		     size_t value_size)
{
	if (buffer == NULL || value == NULL)
		return false;

	if (value_size > buffer_size)
		return false;

	for (size_t i = 0; i <= buffer_size - value_size; i++) {
		if (memcmp((const char *)buffer + i, value, value_size) == 0)
			return true;
	}
	return false;
}

/* Free a malloc'd memory dump structure */
void free_mem_dump(struct mem_dump *dump)
{
	for (int i = 0; i < dump->count; i++)
		free(dump->segments[i].mem);
	free(dump->segments);
}

/*
 * This util function is similar to memcpy, but uses a mem_dump struct
 * as the source memory. The requested memory may span multiple memory segments.
 * The memory segments are not ordered.
 */
void *memcpy_from_dump(struct mem_dump *dump, void *dest,
		       const uint32_t src_addr, size_t size)
{
	size_t offset = 0;

	while (offset < size) {
		int seg;
		/* Find the memory segment that contains the source
		 * address + offset.
		 */
		for (seg = 0; seg < dump->count; seg++) {
			if (src_addr + offset >= dump->segments[seg].addr &&
			    src_addr + offset <
				    dump->segments[seg].addr +
					    dump->segments[seg].size)
				break;
		}
		/* Requested memory not found */
		if (seg >= dump->count)
			return NULL;
		/* Size of the memory segment, starting from the source address
		 */
		size_t segment_size =
			dump->segments[seg].size -
			(src_addr + offset - dump->segments[seg].addr);
		/* Clamp copy size to min of remaning size and segment_size */
		size_t copy_size = MIN(size - offset, segment_size);

		zassert_not_null(memcpy(dest + offset,
					dump->segments[seg].mem + offset,
					copy_size));
		offset += copy_size;
	}

	return dest;
}

/* Util function for fetching a memory dump using host commands. */
int fetch_memory_dump(struct mem_dump *dump)
{
	int rv;
	struct ec_response_get_memory_dump_metadata metadata_response;
	struct host_cmd_handler_args meta_args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_MEMORY_DUMP_METADATA, 0, metadata_response);

	rv = host_command_process(&meta_args);
	zassert_equal(EC_RES_SUCCESS, rv);

	dump->count = metadata_response.memory_dump_entry_count;
	dump->segments = (struct mem_segment *)malloc(
		sizeof(struct mem_segment) *
		metadata_response.memory_dump_entry_count);

	for (int seg = 0; seg < metadata_response.memory_dump_entry_count;
	     seg++) {
		struct ec_params_get_memory_dump_entry_info entry_info_params = {
			.memory_dump_info_index = seg
		};
		struct ec_response_get_memory_dump_entry_info
			entry_info_response;
		struct host_cmd_handler_args entry_args = BUILD_HOST_COMMAND(
			EC_CMD_GET_MEMORY_DUMP_ENTRY_INFO, 0,
			entry_info_response, entry_info_params);

		rv = host_command_process(&entry_args);
		zassert_equal(EC_RES_SUCCESS, rv);

		dump->segments[seg].addr = entry_info_response.address;
		dump->segments[seg].size = entry_info_response.size;
		dump->segments[seg].mem =
			(uint8_t *)malloc(entry_info_response.size);

		uint32_t offset = 0;

		while (offset < entry_info_response.size) {
			struct ec_params_read_memory_dump read_mem_params = {
				.memory_dump_info_index = seg,
				.address = entry_info_response.address + offset,
				.size = entry_info_response.size - offset,
			};
			struct ec_response_read_memory_dump read_mem_response;
			struct host_cmd_handler_args read_args =
				BUILD_HOST_COMMAND(EC_CMD_READ_MEMORY_DUMP, 0,
						   read_mem_response,
						   read_mem_params);

			rv = host_command_process(&read_args);
			zassert_equal(EC_RES_SUCCESS, rv);

			zassert_not_null(memcpy(
				dump->segments[seg].mem + offset,
				read_mem_response.mem, read_mem_response.size));

			offset += read_mem_response.size;
		};
	}
	return rv;
}

/*
 * Ensure that a memory dump returns unavailable if requested before being
 * initialized.
 */
ZTEST_USER(memory_dump, dump_before_initialized)
{
	struct ec_response_get_memory_dump_metadata metadata_response;
	int rv;

	struct host_cmd_handler_args args = BUILD_HOST_COMMAND_RESPONSE(
		EC_CMD_GET_MEMORY_DUMP_METADATA, 0, metadata_response);

	rv = host_command_process(&args);

	zassert_equal(EC_RES_UNAVAILABLE, rv);
}

/* Check if thread stack is included in memory dump */
ZTEST_USER(memory_dump, dump_thread_stack)
{
	const uint32_t magic_val_1 = 0x11111111;
	const uint32_t magic_val_2 = 0x22222222;
	const uint32_t magic_val_3 = 0x33333333;
	struct mem_dump dump;
	uint8_t *test_stack_from_dump;
	struct k_thread test_thread_data;

	/* Create a new thread and pass magic values as initial parameters */
	k_tid_t test_thread = k_thread_create(
		&test_thread_data, test_stack,
		K_THREAD_STACK_SIZEOF(test_stack), test_thread_entry,
		(void *)magic_val_1, (void *)magic_val_2, (void *)magic_val_3,
		1, 0, K_NO_WAIT);

	/* Wait for the thread to start */
	k_msleep(100);

	zassert_true(buffer_contains((uint8_t *)test_thread->stack_info.start,
				     test_thread->stack_info.size, &magic_val_1,
				     4));
	zassert_true(buffer_contains((uint8_t *)test_thread->stack_info.start,
				     test_thread->stack_info.size, &magic_val_2,
				     4));
	zassert_true(buffer_contains((uint8_t *)test_thread->stack_info.start,
				     test_thread->stack_info.size, &magic_val_3,
				     4));

	/* Trigger a memory dump capture */
	initialize_memory_dump();

	/* Stop test thread */
	k_thread_abort(test_thread);

	/* Fetch memory dump */
	fetch_memory_dump(&dump);

	/* Allocate buffer for thread stack */
	test_stack_from_dump = (uint8_t *)malloc(test_thread->stack_info.size);

	zassert_not_null(test_stack_from_dump);

	/* Copy stack from memory dump */
	zassert_not_null(memcpy_from_dump(&dump, test_stack_from_dump,
					  test_thread->stack_info.start,
					  test_thread->stack_info.size));

	/* Search for magic values in fetched stack memory */
	zassert_true(buffer_contains(test_stack_from_dump,
				     test_thread->stack_info.size, &magic_val_1,
				     4));
	zassert_true(buffer_contains(test_stack_from_dump,
				     test_thread->stack_info.size, &magic_val_2,
				     4));
	zassert_true(buffer_contains(test_stack_from_dump,
				     test_thread->stack_info.size, &magic_val_3,
				     4));

	/* Cleanup */
	free(test_stack_from_dump);
	free_mem_dump(&dump);
}

ZTEST_SUITE(memory_dump, NULL, NULL, before, NULL, NULL);
