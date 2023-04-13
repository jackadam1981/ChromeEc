/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "comm-host.h"
#include "misc_util.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <chromeos/ec/panic_defs.h>

#define CROS_EC_DEV_NAME "cros_ec"

bool verbose = false;

struct mem_segment {
	uint32_t addr_start;
	uint32_t addr_end;
	uint32_t size;
	uint8_t *mem;
	struct mem_segment *next;
};


#define COREDUMP_HDR_VER		1

#define	COREDUMP_ARCH_HDR_ID		'A'

#define	COREDUMP_MEM_HDR_ID		'M'
#define COREDUMP_MEM_HDR_VER		1

/* Target code */
enum coredump_tgt_code {
	COREDUMP_TGT_UNKNOWN = 0,
	COREDUMP_TGT_X86,
	COREDUMP_TGT_X86_64,
	COREDUMP_TGT_ARM_CORTEX_M,
	COREDUMP_TGT_RISC_V,
	COREDUMP_TGT_XTENSA,
	COREDUMP_TGT_NDS32,
};

struct arm_arch_block {
	struct {
		uint32_t	r0;
		uint32_t	r1;
		uint32_t	r2;
		uint32_t	r3;
		uint32_t	r12;
		uint32_t	lr;
		uint32_t	pc;
		uint32_t	xpsr;
		uint32_t	sp;

		/* callee registers - optionally collected in V2 */
		uint32_t	r4;
		uint32_t	r5;
		uint32_t	r6;
		uint32_t	r7;
		uint32_t	r8;
		uint32_t	r9;
		uint32_t	r10;
		uint32_t	r11;
	} r;
} __packed;

/* Coredump header */
struct coredump_hdr_t {
	/* 'Z', 'E' */
	char		id[2];

	/* Header version */
	uint16_t	hdr_version;

	/* Target code */
	uint16_t	tgt_code;

	/* Pointer size in Log2 */
	uint8_t		ptr_size_bits;

	uint8_t		flag;

	/* Coredump Reason given */
	unsigned int	reason;
} __packed;

/* Architecture-specific block header */
struct coredump_arch_hdr_t {
	/* COREDUMP_ARCH_HDR_ID */
	char		id;

	/* Header version */
	uint16_t	hdr_version;

	/* Number of bytes in this block (excluding header) */
	uint16_t	num_bytes;
} __packed;

/* Memory block header */
struct coredump_mem32_hdr_t {
	/* COREDUMP_MEM_HDR_ID */
	char		id;

	/* Header version */
	uint16_t	hdr_version;

	/* Address of start of memory region */
	uint32_t	start;

	/* Address of end of memory region */
	uint32_t	end;
} __packed;

struct coredump_mem64_hdr_t {
	/* COREDUMP_MEM_HDR_ID */
	char		id;

	/* Header version */
	uint16_t	hdr_version;

	/* Address of start of memory region */
	uint64_t	start;

	/* Address of end of memory region */
	uint64_t	end;
} __packed;

static int write_zephyr_coredump_memory_block(struct mem_segment *segment, std::ofstream& output_file)
{
	struct coredump_mem32_hdr_t hdr = {
		.id = COREDUMP_MEM_HDR_ID,
		.hdr_version = COREDUMP_MEM_HDR_VER,
		.start = segment->addr_start,
		.end = segment->addr_end,
	};

	if (verbose)
		std::cout << "Memory block:\n"
			  << "\tStart: " << std::hex << hdr.start << std::endl
			  << "\tEnd: " << std::hex << hdr.end << std::endl;

	output_file.write((char *)&hdr, sizeof(hdr));
	output_file.write((char *)segment->mem, segment->size);

	return 0;
}

static int write_zephyr_coredump_cortex_arch_info(struct panic_data *pdata, std::ofstream& output_file)
{
	struct arm_arch_block arch_blk;

	struct coredump_arch_hdr_t hdr = {
		.id = COREDUMP_ARCH_HDR_ID,
		.hdr_version = 2,
		.num_bytes = sizeof(arch_blk),
	};

	if (verbose) {
		std::cout << "Writing coredump arch info..." << std::endl;
		std::cout << "num_bytes: " << hdr.num_bytes << std::endl;
	}


	(void)memset(&arch_blk, 0, sizeof(arch_blk));

	arch_blk.r.r0 = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_R0];
	arch_blk.r.r1 = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_R1];
	arch_blk.r.r2 = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_R2];
	arch_blk.r.r3 = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_R3];
	arch_blk.r.r12 = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_R12];
	arch_blk.r.lr = pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_LR];
	arch_blk.r.pc =  pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_PC];
	arch_blk.r.xpsr =  pdata->cm.frame[CORTEX_PANIC_FRAME_REGISTER_PSR];

	arch_blk.r.sp = pdata->cm.regs[CORTEX_PANIC_REGISTER_PSP];
	arch_blk.r.r4 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R4];
	arch_blk.r.r5 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R5];
	arch_blk.r.r6 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R6];
	arch_blk.r.r7 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R7];
	arch_blk.r.r8 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R8];
	arch_blk.r.r9 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R9];
	arch_blk.r.r10 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R10];
	arch_blk.r.r11 = pdata->cm.regs[CORTEX_PANIC_REGISTER_R11];

	output_file.write((char *)&hdr, sizeof(hdr));
	output_file.write((char *)&arch_blk, sizeof(arch_blk));

	return 0;
}

static int write_zephyr_coredump_header(struct panic_data *pdata, std::ofstream& output_file)
{
	struct coredump_hdr_t hdr = {
		.id = {'Z', 'E'},
		.hdr_version = COREDUMP_HDR_VER,
	};

	if (verbose)
		std::cout << "Writing coredump header..." << std::endl;

	switch (pdata->arch) {
	case PANIC_ARCH_CORTEX_M:
		hdr.tgt_code = COREDUMP_TGT_ARM_CORTEX_M;
		hdr.ptr_size_bits = 5; /* 2^5 = 32 */
		hdr.reason = pdata->cm.regs[CORTEX_PANIC_REGISTER_R4];
		break;
	case PANIC_ARCH_NDS32_N8:
		hdr.tgt_code = COREDUMP_TGT_NDS32;
		hdr.ptr_size_bits = 5; /* 2^5 = 32 */
		break;
	case PANIC_ARCH_RISCV_RV32I:
		hdr.tgt_code = COREDUMP_TGT_RISC_V;
		hdr.ptr_size_bits = 5; /* 2^5 = 32 */
		hdr.reason = pdata->cm.regs[11];
		break;
	default:
		std::cerr << "ERROR: Unknown architecture" << std::endl;
		return -1;
	}

	output_file.write((char *)&hdr, sizeof(hdr));

	return 0;
}

static int get_panic_info(struct panic_data *pdata)
{
	int size;

	if (verbose)
		std::cout << "Getting panic info..." << std::endl;

	size = ec_command(EC_CMD_GET_PANIC_INFO, 0, NULL, 0,
			ec_inbuf, ec_max_insize);
	if (size < 0) {
		std::cerr << "Error: Failed to get panic info" << std::endl;
		return -1;
	}

	if (size == 0) {
		std::cerr << "Error: Panic info is empty" << std::endl;
		return -1;
	}

	if (size > sizeof(*pdata)) {
		if (verbose) {
			std::cerr << "Warning: Panic larger than expected" << std::endl;
		}
		size = sizeof(*pdata);
	}

	memcpy(pdata, ec_inbuf, size);

	if (pdata->struct_version > 2 || pdata->struct_version == 0) {
    		std::cerr << "Error: Unexpected struct version: " << pdata->struct_version << std::endl;
		return -1;
	}

	if (pdata->reserved != 0) {
		std::cerr << "Error: Unexpected panic reserve value: " << pdata->reserved << std::endl;
		return -1;
	}

	return 0;
}

struct mem_segment *get_segments(void)
{
	int rv;
	uint32_t requested_address_start = 0;
	uint32_t requested_address_end = UINT32_MAX;
	/* Simple local structs for storing a memory dump */
	uint16_t entry_count;
	struct mem_segment *segments = NULL;
	/* The real root is root.next, all other root fields are unused */
	struct mem_segment root;
	struct mem_segment *seg;
	struct ec_response_memory_dump_get_metadata metadata_response;

	/* Fetch memory dump metadata */
	rv = ec_command(EC_CMD_MEMORY_DUMP_GET_METADATA, 0, NULL, 0,
			&metadata_response, sizeof(metadata_response));
	if (rv < 0) {
		std::cerr << "Error: Failed to get memory dump metadata from EC"
			  << std::endl;
		goto cmd_memory_dump_cleanup;
	}
	entry_count = metadata_response.memory_dump_entry_count;
	if (entry_count == 0) {
		std::cerr << "Error: EC memory dump is empty" << std::endl;
		goto cmd_memory_dump_cleanup;
	}
	if (verbose)
		std::cout << "Fetching " << entry_count
			  << " memory dump entries" << std::endl;

	segments = (struct mem_segment *)malloc(sizeof(struct mem_segment) *
						entry_count);
	if (segments == NULL) {
		std::cerr << "Error: malloc failed" << std::endl;
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
			std::cerr << "Error: Failed to get memory dump info for entry "
				  << std::endl;
			goto cmd_memory_dump_cleanup;
		}

		uint32_t entry_address_end =
			entry_info_response.address + entry_info_response.size;

		if (verbose)
			std::cout << "Entry info: " << std::endl
				  << "\tStart address: "
				  << std::hex << entry_info_response.address << std::endl
				  << "\tEnd address: " << std::hex << entry_address_end
				  << std::endl
				  << "\tSize: " << std::hex << entry_info_response.size
				  << std::endl;
		// start_address: " << std::hex <<  entry_info_response.address
		// << std::endl;

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

		seg->mem = (uint8_t *)malloc(seg->size);
		if (seg->mem == NULL) {
			std::cerr << "Error: malloc failed\n";
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
				std::cerr << "Error: Failed to read EC memory at "
					  << std::hex << read_mem_params.address
					  << std::endl;
				goto cmd_memory_dump_cleanup;
			}

			if (!memcpy(seg->mem + offset, ec_inbuf, rv)) {
				std::cerr << "Error: memcpy failed" << std::endl;
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
			std::cerr << "Error: Segment size is not aligned" << std::endl;
			goto cmd_memory_dump_cleanup;
		}
		seg->mem = (uint8_t *)realloc(seg->mem, new_size);
		if (seg->mem == NULL) {
			std::cerr << "Error: realloc failed" << std::endl;
			goto cmd_memory_dump_cleanup;
		}
		if (!memcpy(seg->mem + seg->size, seg->next->mem + overlap,
			    seg->next->size - overlap)) {
			std::cerr << "Error: memcpy failed" << std::endl;
			goto cmd_memory_dump_cleanup;
		}
		seg->addr_end = seg->next->addr_end;
		seg->size = new_size;
		seg->next = seg->next->next;
	}

	return root.next;
cmd_memory_dump_cleanup:
	if (segments) {
		for (int i = 0; i < entry_count; i++)
			free(segments[i].mem);
		free(segments);
	}
	return nullptr;
}

void print_help() {
    std::cout << "Usage: prog_name [OPTIONS] OUTPUT_FILENAME\n"
	      << "Options:\n"
	      << "  -v, --verbose\tDisplay verbose output\n"
	      << "  -h, --help\tShow this help message and exit\n";
}

int main(int argc, char *argv[])
{
	int rv;
	std::vector<std::string> args(argv + 1, argv + argc);
	std::string output_filename;
	struct panic_data pdata;

	for (size_t i = 0; i < args.size(); ++i) {
		if (args[i] == "-v" || args[i] == "--verbose") {
			verbose = true;
		} else if (args[i] == "-h" || args[i] == "--help") {
			print_help();
			return 0;
		} else if (output_filename.empty()) {
			output_filename = args[i];
		} else {
			std::cerr << "Error: Invalid argument '" << args[i]
				  << "'.\n";
			return -1;
		}
	}

	if (comm_init_dev(CROS_EC_DEV_NAME)) {
		std::cerr << "Error: Failed to initialize " << CROS_EC_DEV_NAME
			  << std::endl;
		return -1;
	}

	if (comm_init_buffer()) {
		std::cerr << "Error: Failed to initialize buffers\n";
		return -1;
	}

	if (output_filename.empty()) {
		std::cerr << "Error: No output filename provided" << std::endl;
		return -1;
	}
	std::ofstream output_file(output_filename);
	if (!output_file) {
		std::cerr << "Error: Unable to open output file '"
			  << output_filename << "'" << std::endl;
		return -1;
	}
	if (verbose) {
		std::cout << "Opened output file: '" << output_filename << std::endl;
	}

	rv = get_panic_info(&pdata);
	if (rv != 0) {
		std::cerr << "Error: Failed to get panic info" << std::endl;
		return -1;
	}

	rv = write_zephyr_coredump_header(&pdata, output_file);
	if (rv != 0) {
		std::cerr << "Error: Failed to write zephyr coredump header" << std::endl;
		return -1;
	}

	switch (pdata.arch) {
	case PANIC_ARCH_CORTEX_M:
		rv = write_zephyr_coredump_cortex_arch_info(&pdata, output_file);
		break;
	default:
		std::cerr << "ERROR: Unhandled architecture" << std::endl;
		return -1;
	}
	if (rv != 0) {
		std::cerr << "Error: Failed to write zephyr coredump arch info" << std::endl;
		return -1;
	}


	struct mem_segment *segments = get_segments();
	if (segments == nullptr) {
		std::cerr << "Error: Failed to get segments" << std::endl;
		return -1;
	}

	struct mem_segment *current_segment = segments;
	while(current_segment != nullptr) {
		rv = write_zephyr_coredump_memory_block(current_segment, output_file);
		if (rv != 0) {
			std::cerr << "Error: Failed to write zephyr coredump memory block" << std::endl;
			return -1;
		}
		current_segment = current_segment->next;
	}

	output_file.close();

	if (verbose) {
		std::cout << "Closed output file: '" << output_filename << "'" << std::endl;
	}

	return 0;
}
