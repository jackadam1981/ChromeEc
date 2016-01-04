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
	uint32_t  block_base;
	uint32_t  block_digest;
	uint8_t   block_body[0];
} __packed;

const struct section_descriptor {
	uint32_t sect_base_offset;
	uint32_t sect_top_offset;
} rw_sections[] = {
	{CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RW_MEM_OFF,
		 CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RW_MEM_OFF +
		 CONFIG_RW_SIZE},
	{CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RW_B_MEM_OFF,
		 CONFIG_PROGRAM_MEMORY_BASE + CONFIG_RW_B_MEM_OFF +
		 CONFIG_RW_SIZE}
};

const struct section_descriptor *valid_section;

static int valid_upgrade_chunk(struct upgrade_command *cmd_body, size_t body_size)
{
	if (!valid_section) {
		int i;
		uint32_t run_time_addr = (uint32_t) valid_upgrade_chunk;

		for (i = 0; i < ARRAY_SIZE(rw_sections); i++) {
			if ((run_time_addr > rw_sections[i].sect_base_offset) ||
			    (run_time_addr < rw_sections[i].sect_top_offset)) {
				valid_section = rw_sections + i;
				break;
			}
		}
		if (!valid_section)
			return 0;
	}

	if ((cmd_body->block_base >= valid_section->sect_base_offset) &&
	    ((cmd_body->block_base + body_size) <
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

	cmd_body->block_base = be32toh(cmd_body->block_base);
	*response_size = sizeof(*rv);

	body_size = cmd_size - offsetof(struct upgrade_command, block_body);
	if (body_size < 0) {
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
			*rv = upgrade_gen_error;
			return;
		}
		if (flash_physical_erase(valid_section->sect_base_offset,
					 valid_section->sect_top_offset -
					 valid_section->sect_base_offset)) {
			*rv = upgrade_erase_failure;
		} else {
			/*
			 * Successful erase means that we need to return the
			 * base address of the section to be programmed with
			 * the upgrade.
			 */
			*(uint32_t *)body = htobe32(valid_section->
						    sect_base_offset);
			*response_size = sizeof(uint32_t);
		}
		return;
	}

	if (valid_upgrade_chunk(cmd_body, body_size)) {
		*rv = upgrade_bad_addr;
		return;
	}

	DCRYPTO_SHA1_hash(cmd_body->block_body, body_size, sha1_digest);
	if (memcmp(sha1_digest, &cmd_body->block_digest,
		   sizeof(cmd_body->block_digest))) {
		*rv = upgrade_data_error;
		CPRINTF("%s:%d calculated %x not equal received %x\n", __func__, __LINE__,
			*(uint32_t *)sha1_digest, cmd_body->block_digest);
		return;
	}

	if (flash_physical_write(cmd_body->block_base -
				 CONFIG_PROGRAM_MEMORY_BASE,
				 body_size,
				 cmd_body->block_body) != EC_SUCCESS) {
		*rv = upgrade_write_failure;
	} else {
		if (memcmp(cmd_body->block_body, (void *)cmd_body->block_base,
			   body_size))
			*rv = upgrade_verify_error;
		else
			*rv = upgrade_success;
	}
}

DECLARE_EXTENSION_COMMAND(EXTENSION_FW_UPGRADE, fw_upgrade_command_handler);
