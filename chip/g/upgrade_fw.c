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
#include "memory.h"
#include "system.h"
#include "registers.h"
#include "uart.h"

#include "upgrade_fw.h"
#include "cryptoc/sha.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

/* Various upgrade extension command return values. */
enum return_value {
	UPGRADE_SUCCESS = 0,
	UPGRADE_BAD_ADDR = 1,
	UPGRADE_ERASE_FAILURE = 2,
	UPGRADE_DATA_ERROR = 3,
	UPGRADE_WRITE_FAILURE = 4,
	UPGRADE_VERIFY_ERROR = 5,
	UPGRADE_GEN_ERROR = 6,
};

/*
 * This structure defines flash offset ranges of the RO and RW images which
 * are not currently active and as such could be overwritten with an
 * update.
 */
struct {
	uint32_t ro_base_offset;
	uint32_t ro_top_offset;
	uint32_t rw_base_offset;
	uint32_t rw_top_offset;
} valid_sections;

/* Pick sections where updates can go to based on current code addresses. */
static void set_valid_sections(void)
{
	switch (system_get_ro_image_copy()) {
	case SYSTEM_IMAGE_RO:
		valid_sections.ro_base_offset = CHIP_RO_B_MEM_OFF;
		break;
	case SYSTEM_IMAGE_RO_B:
		valid_sections.ro_base_offset = CONFIG_RO_MEM_OFF;
		break;
	default:
		CPRINTF("Failed to set RO image offsets\n");
		return;
	}

	switch (system_get_image_copy()) {
	case SYSTEM_IMAGE_RW:
		valid_sections.rw_base_offset = CONFIG_RW_B_MEM_OFF;
		break;
	case SYSTEM_IMAGE_RW_B:
		valid_sections.rw_base_offset = CONFIG_RW_MEM_OFF;
		break;
	default:
		CPRINTF("Failed to set RW image offsets\n");
		return;
	}

	valid_sections.ro_top_offset = valid_sections.ro_base_offset +
		CONFIG_RO_SIZE - 0x800; /* 2K for certs! */

	valid_sections.rw_top_offset = valid_sections.rw_base_offset +
		CONFIG_RW_SIZE;
}

/* Verify that the passed in block fits into the valid area. */
static int valid_upgrade_chunk(uint32_t block_offset, size_t body_size)
{
	/* Is this an RW chunk? */
	if (valid_sections.rw_top_offset &&
	    (block_offset >= valid_sections.rw_base_offset) &&
	    ((block_offset + body_size) <= valid_sections.rw_top_offset))
		return 1;

	/* Is this an RO chunk? */
	if (valid_sections.ro_top_offset &&
	    (block_offset >= valid_sections.ro_base_offset) &&
	    ((block_offset + body_size) <= valid_sections.ro_top_offset))
		return 1;
	return 0;
}

static void open_ro_window(uint32_t offset, size_t size_b)
{
	GREG32(GLOBALSEC, FLASH_REGION6_BASE_ADDR) =
		offset + CONFIG_PROGRAM_MEMORY_BASE;
	GREG32(GLOBALSEC, FLASH_REGION6_SIZE) = size_b - 1;
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, RD_EN, 1);
	GWRITE_FIELD(GLOBALSEC, FLASH_REGION6_CTRL, WR_EN, 1);
}

void fw_upgrade_command_handler(void *body,
				size_t cmd_size,
				size_t *response_size)
{
	struct upgrade_command *cmd_body = body;
	uint8_t *error_code = body;  /* Cache the address for code clarity. */
	uint8_t sha1_digest[SHA_DIGEST_SIZE];
	size_t body_size;
	uint32_t block_offset;

	*response_size = 1; /* One byte response unless this is a start PDU. */

	if (cmd_size < offsetof(struct upgrade_command, block_body)) {
		CPRINTF("%s:%d\n", __func__, __LINE__);
		*error_code = UPGRADE_GEN_ERROR;
		return;
	}

	body_size = cmd_size - offsetof(struct upgrade_command, block_body);

	if (!cmd_body->block_base && !body_size) {
		struct first_response_pdu *rpdu = body;
		uint32_t base;
		uint32_t size;

		/*
		 * This is the first message of the upgrade process, let's
		 * determine the valid upgrade sections and erase their
		 * contents.
		 */
		set_valid_sections();

		/* Prepare response structure. */
		memset(rpdu, 0, sizeof(*rpdu));
		*response_size = sizeof(*rpdu);

		if (!valid_sections.ro_top_offset ||
		    !valid_sections.rw_top_offset) {
			rpdu->return_value = htobe32(UPGRADE_GEN_ERROR);
			return;
		}

		base = valid_sections.ro_base_offset;
		size = valid_sections.ro_top_offset -
			valid_sections.ro_base_offset;

		open_ro_window(base, size);
		if (flash_erase(base, size) != EC_SUCCESS) {
		    CPRINTF("%s:%d erase failure of 0x%x..+0x%x\n",
			    __func__, __LINE__, base, size);
		    rpdu->return_value = htobe32(UPGRADE_ERASE_FAILURE);
		    return;
		}

		base = valid_sections.rw_base_offset;
		size = valid_sections.rw_top_offset -
			valid_sections.rw_base_offset;
		if (flash_erase(base, size) != EC_SUCCESS) {
			CPRINTF("%s:%d erase failure of 0x%x..+0x%x\n",
				__func__, __LINE__, base, size);
			rpdu->return_value = htobe32(UPGRADE_ERASE_FAILURE);
			return;
		}

		/*
		  crosbug.com/p/54916
		  wipe_nvram(); Do not keep any state around.
		*/

		/*
		 * We need to return the base addresses of the sections to be
		 * programmed with the upgrade.
		 */


		rpdu->vers3.backup_ro_offset =
			htobe32(valid_sections.ro_base_offset +
				CONFIG_PROGRAM_MEMORY_BASE);
		rpdu->vers3.backup_rw_offset =
			htobe32(valid_sections.rw_base_offset +
				CONFIG_PROGRAM_MEMORY_BASE);
		rpdu->protocol_version = htobe32(UPGRADE_PROTOCOL_VERSION);
		return;
	}

	/* Check if the block will fit into the valid area. */
	block_offset = be32toh(cmd_body->block_base) -
		CONFIG_PROGRAM_MEMORY_BASE;
	if (!valid_upgrade_chunk(block_offset, body_size)) {
		*error_code = UPGRADE_BAD_ADDR;
		CPRINTF("%s:%d %x, %d base %x top %x\n", __func__, __LINE__,
			block_offset, body_size,
			valid_sections.rw_base_offset,
			valid_sections.rw_top_offset);
		return;
	}

	/* Check if the block was received properly. */
	DCRYPTO_SHA1_hash((uint8_t *)&cmd_body->block_base,
			  body_size + sizeof(cmd_body->block_base),
			  sha1_digest);
	if (memcmp(sha1_digest, &cmd_body->block_digest,
		   sizeof(cmd_body->block_digest))) {
		*error_code = UPGRADE_DATA_ERROR;
		CPRINTF("%s:%d sha1 %x not equal received %x at offs. 0x%x\n",
			__func__, __LINE__,
			*(uint32_t *)sha1_digest, cmd_body->block_digest,
			block_offset);
		return;
	}

	CPRINTF("%s: programming at address 0x%x\n", __func__,
		block_offset + CONFIG_PROGRAM_MEMORY_BASE);
	if (flash_physical_write(block_offset, body_size,
				 cmd_body->block_body) != EC_SUCCESS) {
		*error_code = UPGRADE_WRITE_FAILURE;
		CPRINTF("%s:%d upgrade write error\n",
			__func__, __LINE__);
		return;
	}

	/* Werify that data was written properly. */
	if (memcmp(cmd_body->block_body, (void *)
		   (block_offset + CONFIG_PROGRAM_MEMORY_BASE),
		   body_size)) {
		*error_code = UPGRADE_VERIFY_ERROR;
		CPRINTF("%s:%d upgrade verification error\n",
			__func__, __LINE__);
		return;
	}

	*error_code = UPGRADE_SUCCESS;
}
