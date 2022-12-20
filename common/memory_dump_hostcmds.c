
/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "memory_dump_hostcmds.h"
#include "string.h"

#include <zephyr/arch/cpu.h>
#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>

#define MAX_DUMP_ENTRIES 64

struct memory_dump_entry {
	uint32_t address;
	uint32_t size;
};

static struct memory_dump_entry entries[MAX_DUMP_ENTRIES];
static uint16_t memory_dump_entry_count;
static uintptr_t memory_dump_address_offset;
static uint32_t memory_dump_timestamp;
static bool memory_dump_initialized;

static void register_thread_memory_dump(const struct k_thread *thread,
					void *user_data)
{
	ARG_UNUSED(user_data);

	if (memory_dump_entry_count >= MAX_DUMP_ENTRIES)
		printk("Memory dump exceeds max");

	entries[memory_dump_entry_count].address =
		thread->stack_info.start - memory_dump_address_offset;
	entries[memory_dump_entry_count].size =
		thread->stack_info.size - thread->stack_info.delta;

	memory_dump_entry_count += 1;
}

int initialize_memory_dump(void)
{
	memory_dump_timestamp = k_cycle_get_32();
	memory_dump_address_offset = 0;

	k_thread_foreach_unlocked(register_thread_memory_dump, NULL);

	memory_dump_initialized = true;

	return EC_SUCCESS;
}

int clear_memory_dump(void)
{
	memory_dump_initialized = false;
	memory_dump_entry_count = 0;

	return EC_SUCCESS;
}

static enum ec_status
get_memory_dump_metadata(struct host_cmd_handler_args *args)
{
	struct ec_response_get_memory_dump_metadata *r = args->response;

	if (!memory_dump_initialized)
		return EC_RES_UNAVAILABLE;

	r->memory_dump_entry_count = memory_dump_entry_count;
	r->memory_dump_address_offset = memory_dump_address_offset;
	r->memory_dump_timestamp = memory_dump_timestamp;
	r->memory_dump_total_size = 0;
	for (int i = 0; i < memory_dump_entry_count; i++)
		r->memory_dump_total_size += entries[i].size;

	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_MEMORY_DUMP_METADATA, get_memory_dump_metadata,
		     EC_VER_MASK(0));

static enum ec_status
get_memory_dump_entry_info(struct host_cmd_handler_args *args)
{
	const struct ec_params_get_memory_dump_entry_info *p = args->params;
	struct ec_response_get_memory_dump_entry_info *r = args->response;

	if (!memory_dump_initialized)
		return EC_RES_UNAVAILABLE;

	if (p->memory_dump_info_index >= memory_dump_entry_count ||
	    p->memory_dump_info_index >= MAX_DUMP_ENTRIES)
		return EC_RES_ACCESS_DENIED;

	r->address = entries[p->memory_dump_info_index].address;
	r->size = entries[p->memory_dump_info_index].size;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_MEMORY_DUMP_ENTRY_INFO,
		     get_memory_dump_entry_info, EC_VER_MASK(0));

static enum ec_status read_memory_dump(struct host_cmd_handler_args *args)
{
	const struct ec_params_read_memory_dump *p = args->params;
	struct ec_response_read_memory_dump *r = args->response;
	struct memory_dump_entry entry;

	if (!memory_dump_initialized)
		return EC_RES_UNAVAILABLE;

	if (p->memory_dump_info_index >= memory_dump_entry_count ||
	    p->memory_dump_info_index >= MAX_DUMP_ENTRIES)
		return EC_RES_ACCESS_DENIED;

	entry = entries[p->memory_dump_info_index];

	if (p->address < entry.address ||
	    p->address + p->size > entry.address + entry.size)
		return EC_RES_ACCESS_DENIED;

	r->size = MIN(p->size, EC_MAX_MEMORY_DUMP_READ_SIZE);

	memcpy(r->mem, (void *)(p->address + memory_dump_address_offset),
	       r->size);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_READ_MEMORY_DUMP, read_memory_dump, EC_VER_MASK(0));
