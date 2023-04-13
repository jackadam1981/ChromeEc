/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "comm-host.h"
#include "misc_util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define CROS_EC_DEV_NAME "cros_ec"

static void cmd_memory_dump_usage(const char *command_name)
{
	fprintf(stderr,
		"Usage: %s [<address> [<size>]]\n"
		"  Prints the memory available for dumping in hexdump cononical format.\n"
		"  <address> is a 32-bit address offset. Defaults to 0x0.\n"
		"  <size> is the number of bytes to print after <address>."
		" Defaults to end of RAM.\n"
		"Usage: %s info\n"
		"  Prints metadata about the memory available for dumping\n",
		command_name, command_name);
}

int main(int argc, char *argv[])
{
	int rv;
	char *e;
	bool just_info;
	const char *command_name = argv[0];
	uint32_t requested_address_start = 0;
	uint32_t requested_address_end = UINT32_MAX;
	/* Simple local structs for storing a memory dump */
	struct mem_segment {
		uint32_t addr_start;
		uint32_t addr_end;
		uint32_t size;
		uint8_t *mem;
		struct mem_segment *next;
	};
	uint16_t entry_count;
	struct mem_segment *segments = NULL;
	/* The real root is root.next, all other root fields are unused */
	struct mem_segment root;
	struct mem_segment *seg;
	struct ec_response_memory_dump_get_metadata metadata_response;

	if (comm_init_dev(CROS_EC_DEV_NAME)) {
		fprintf(stderr, "Couldn't initialize %s\n", CROS_EC_DEV_NAME);
		return -2;
	}

	if (comm_init_buffer()) {
		fprintf(stderr, "Couldn't initialize buffers\n");
		return -3;
	}

	if (argc > 3 || (argc == 2 && strcmp(argv[1], "help") == 0)) {
		cmd_memory_dump_usage(command_name);
		return -4;
	}
	if (argc == 2 && strcmp(argv[1], "info") == 0) {
		just_info = true;
	}
	if (argc >= 2 && !just_info) {
		/* Parse requested address argument */
		requested_address_start = strtoul(argv[1], &e, 0);
		if (e && *e) {
			fprintf(stderr, "Bad argument '%s'\n", argv[1]);
			cmd_memory_dump_usage(command_name);
			return -5;
		}
	}
	if (argc == 3 && !just_info) {
		/* Parse requested size argument */
		uint32_t requested_size = strtoul(argv[2], &e, 0);
		if (e && *e) {
			fprintf(stderr, "Bad argument '%s'\n", argv[2]);
			cmd_memory_dump_usage(command_name);
			return -6;
		}
		/* Cap max address at UINT32_MAX */
		requested_address_end =
			MIN((uint64_t)requested_address_start + requested_size,
			    (uint64_t)UINT32_MAX);
	}

	/* Fetch memory dump metadata */
	rv = ec_command(EC_CMD_MEMORY_DUMP_GET_METADATA, 0, NULL, 0,
			&metadata_response, sizeof(metadata_response));
	if (rv < 0) {
		fprintf(stderr, "Failed to get memory dump metadata.\n");
		goto cmd_memory_dump_cleanup;
	}
	entry_count = metadata_response.memory_dump_entry_count;
	if (entry_count == 0) {
		fprintf(stderr, "Memory dump is empty.\n");
		rv = -1;
		goto cmd_memory_dump_cleanup;
	}
	segments = (struct mem_segment *)malloc(sizeof(struct mem_segment) *
						entry_count);
	if (segments == NULL) {
		fprintf(stderr, "malloc failed\n");
		rv = -1;
		goto cmd_memory_dump_cleanup;
	}

	/* Fetch all memory segments */
	for (uint16_t entry_index = 0; entry_index < entry_count;
	     entry_index++) {
		seg = &segments[entry_index];
		struct ec_params_memory_dump_get_entry_info entry_info_params = {
			.memory_dump_entry_index = entry_index
		};
		struct ec_response_memory_dump_get_entry_info
			entry_info_response;

		rv = ec_command(EC_CMD_MEMORY_DUMP_GET_ENTRY_INFO, 0,
				&entry_info_params, sizeof(entry_info_params),
				&entry_info_response,
				sizeof(entry_info_response));
		if (rv < 0) {
			fprintf(stderr,
				"Failed to get memory dump info for entry %d.\n",
				entry_index);
			goto cmd_memory_dump_cleanup;
		}

		uint32_t entry_address_end =
			entry_info_response.address + entry_info_response.size;

		/* Check if entry is even in bounds of the requested range */
		if (entry_info_response.address >= requested_address_end ||
		    entry_address_end <= requested_address_start)
			continue;

		/* Clip memory segment boundaries based on requested range */
		seg->addr_start = MAX(entry_info_response.address,
				      requested_address_start);
		seg->addr_end = MIN(entry_address_end, requested_address_end);
		if (seg->addr_end - seg->addr_start <= 0)
			continue;
		seg->size = seg->addr_end - seg->addr_start;

		if (just_info) {
			printf("%-3d: %x-%x (%d bytes)\n", entry_index,
			       seg->addr_start, seg->addr_end, seg->size);
			continue;
		}

		seg->mem = (uint8_t *)malloc(seg->size);
		if (seg->mem == NULL) {
			fprintf(stderr, "malloc failed\n");
			rv = -1;
			goto cmd_memory_dump_cleanup;
		}

		/* Keep fetching until entire segment is copied */
		uint32_t offset = 0;
		while (offset < seg->size) {
			struct ec_params_memory_dump_read_memory
				read_mem_params = {
					.memory_dump_entry_index = entry_index,
					.address = seg->addr_start + offset,
					.size = seg->size - offset,
				};

			rv = ec_command(EC_CMD_MEMORY_DUMP_READ_MEMORY, 0,
					&read_mem_params,
					sizeof(read_mem_params), ec_inbuf,
					ec_max_insize);
			if (rv <= 0) {
				fprintf(stderr,
					"Failed to read memory at %x.\n",
					read_mem_params.address);
				rv = -1;
				goto cmd_memory_dump_cleanup;
			}

			if (!memcpy(seg->mem + offset, ec_inbuf, rv)) {
				fprintf(stderr, "memcpy failed\n");
				rv = -1;
				goto cmd_memory_dump_cleanup;
			}

			offset += rv;
		};

		/* Sort segments in ascending order of starting address */
		struct mem_segment *current = &root;
		for (int i = 0; current->next && i < entry_count; i++) {
			if (seg->addr_start < current->next->addr_start) {
				/* Insert segment before current->next */
				seg->next = current->next;
				current->next = seg;
				break;
			}
			current = current->next;
		}
		current->next = seg;
	}

	if (just_info) {
		rv = 0;
		goto cmd_memory_dump_cleanup;
	}

	/* Merge overlapping or touching segments */
	seg = root.next;
	for (int i = 0; seg && seg->next && i < entry_count; i++) {
		if (seg->addr_end < seg->next->addr_start) {
			/* No overlap */
			seg = seg->next;
			continue;
		}
		uint32_t overlap = seg->addr_end - seg->next->addr_start;
		uint32_t new_size = seg->size + seg->next->size - overlap;
		if (new_size != seg->next->addr_end - seg->addr_start) {
			fprintf(stderr, "Segment size is not aligned\n");
			rv = -1;
			goto cmd_memory_dump_cleanup;
		}
		seg->mem = (uint8_t *)realloc(seg->mem, new_size);
		if (seg->mem == NULL) {
			fprintf(stderr, "realloc failed\n");
			rv = -1;
			goto cmd_memory_dump_cleanup;
		}
		if (!memcpy(seg->mem + seg->size, seg->next->mem + overlap,
			    seg->next->size - overlap)) {
			fprintf(stderr, "Merging segments failed\n");
			rv = -1;
			goto cmd_memory_dump_cleanup;
		}
		seg->addr_end = seg->next->addr_end;
		seg->size = new_size;
		seg->next = seg->next->next;
	}

	/* Print dump in hexdump cononical format */
	seg = root.next;
	for (int i = 0; seg && i < entry_count; i++) {
		hexdump_canonical(seg->mem, seg->size, seg->addr_start);
		/* Extra newline to delinate segments */
		printf("\n");
		seg = seg->next;
	}
	rv = 0;
cmd_memory_dump_cleanup:
	if (segments) {
		for (int i = 0; i < entry_count; i++)
			free(segments[i].mem);
		free(segments);
	}
	return rv;
}
