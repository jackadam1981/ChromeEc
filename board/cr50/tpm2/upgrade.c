/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "byteorder.h"
#include "console.h"
#include "dcrypto/dcrypto.h"
#include "extension.h"
#include "flash.h"
#include "hooks.h"
#include "include/compile_time_macros.h"
#include "sha1.h"
#include "uart.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

enum return_value {
	upgrade_success = 0,
	upgrade_bad_addr = 1,
	upgrade_erase_failure = 2,
	upgrade_data_error = 3,
	upgrade_write_failure = 4,
	upgrade_verify_error = 5,
	upgrade_gen_error = 6,
};

struct upgrade_command {
	uint32_t  block_digest;
	uint32_t  block_base;
	uint8_t   block_body[0];
} __packed;

const struct section_descriptor {
	uint32_t sect_base_offset;
	uint32_t sect_top_offset;
} rw_sections[] = {
	{CONFIG_RW_MEM_OFF,
	 CONFIG_RW_MEM_OFF + CONFIG_RW_SIZE},
	{CONFIG_RW_B_MEM_OFF,
	 CONFIG_RW_B_MEM_OFF + CONFIG_RW_SIZE}
};

const struct section_descriptor *valid_section;

static int valid_upgrade_chunk(uint32_t block_offset, size_t body_size)
{
	if (!valid_section) {
		int i;
		uint32_t run_time_offs = (uint32_t) valid_upgrade_chunk -
			CONFIG_PROGRAM_MEMORY_BASE;

		for (i = 0; i < ARRAY_SIZE(rw_sections); i++) {
			if ((run_time_offs > rw_sections[i].sect_base_offset) &&
			    (run_time_offs < rw_sections[i].sect_top_offset))
				continue;
			valid_section = rw_sections + i;
			CPRINTF("valid section at %x+..%x\n", valid_section->sect_base_offset,
					valid_section->sect_top_offset);
			break;
		}
	}

	/* Check if his is the first invocation, just to set the region. */
	if (!block_offset)
		return !!valid_section;

	if ((block_offset >= valid_section->sect_base_offset) &&
	    ((block_offset + body_size) <
	     valid_section->sect_top_offset))
		return 1;

	return 0;
}

static void fw_upgrade_command_handler(void *body,
				       size_t cmd_size,
				       size_t *response_size)
{
	struct upgrade_command *cmd_body = body;
	uint8_t *rv = body;
	uint8_t sha1_digest[SHA1_DIGEST_SIZE];
	size_t body_size;
	uint32_t block_offset;

	*response_size = sizeof(*rv);

	body_size = cmd_size - offsetof(struct upgrade_command, block_body);
	if (body_size < 0) {
		CPRINTF("%s:%d\n",__func__, __LINE__);
		*rv = upgrade_gen_error;
		return;
	}

	if (!cmd_body->block_base && !body_size) {
		/*
		 * This is the beginning of the upgrade process, let's erase
		 * the alternative RW section.
		 */
		valid_section = NULL;
		valid_upgrade_chunk(0, 0);
		if (!valid_section) {
			CPRINTF("%s:%d invalid upgrade chunk\n",
				__func__, __LINE__);
			*rv = upgrade_gen_error;
			return;
		}
		if (flash_physical_erase(valid_section->sect_base_offset,
					 valid_section->sect_top_offset -
					 valid_section->sect_base_offset)) {
			CPRINTF("%s:%d erase failure of 0x%x..+0x%x\n",
				__func__, __LINE__,
				valid_section->sect_base_offset,
				valid_section->sect_top_offset -
				valid_section->sect_base_offset);
			*rv = upgrade_erase_failure;
		} else {
			/*
			 * Successful erase means that we need to return the
			 * base address of the section to be programmed with
			 * the upgrade.
			 */
			*(uint32_t *)body = htobe32(valid_section->
						    sect_base_offset +
						    CONFIG_PROGRAM_MEMORY_BASE);
			*response_size = sizeof(uint32_t);
		}
		return;
	}

	block_offset = be32toh(cmd_body->block_base) -
		CONFIG_PROGRAM_MEMORY_BASE;
	if (!valid_upgrade_chunk(block_offset, body_size)) {
		*rv = upgrade_bad_addr;
		CPRINTF("%s:%d %x, %d base %x top %x\n",__func__, __LINE__,
			block_offset, body_size,
			valid_section->sect_base_offset, valid_section->sect_top_offset);
		return;
	}

	DCRYPTO_SHA1_hash((uint8_t *)&cmd_body->block_base,
			  body_size + sizeof(cmd_body->block_base),
			  sha1_digest);
	if (memcmp(sha1_digest, &cmd_body->block_digest,
		   sizeof(cmd_body->block_digest))) {
		*rv = upgrade_data_error;
		CPRINTF("%s:%d calculated %x not equal received %x at offset 0x%x\n",
			__func__, __LINE__,
			*(uint32_t *)sha1_digest, cmd_body->block_digest,
			block_offset);
		return;
	}

	CPRINTF("%s: programming at offset 0x%x\n", __func__, block_offset);

	if (flash_physical_write(block_offset, body_size,
				 cmd_body->block_body) != EC_SUCCESS) {
		*rv = upgrade_write_failure;
		CPRINTF("%s:%d upgrade write error\n",
			__func__, __LINE__);
	} else {
		if (memcmp(cmd_body->block_body, (void *)
			   (block_offset + CONFIG_PROGRAM_MEMORY_BASE),
			    body_size)) {
			*rv = upgrade_verify_error;
			CPRINTF("%s:%d upgrade verification error\n",
				__func__, __LINE__);
		} else {
			*rv = upgrade_success;
		}
	}
}

DECLARE_EXTENSION_COMMAND(EXTENSION_FW_UPGRADE, fw_upgrade_command_handler);
