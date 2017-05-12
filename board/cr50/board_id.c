/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board_id.h"
#include "common.h"
#include "console.h"
#include "extension.h"
#include "flash_info.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "scratch_reg1.h"
#include "system.h"
#include "system_chip.h"
#include "task.h"
#include "timer.h"
#include "tpm_registers.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_RBOX, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_RBOX, format, ## args)

/**
 * Return the image header for the current image copy
 */
const struct SignedHeader *get_current_image_header(void)
{
	return (const struct SignedHeader *)
			get_program_memory_addr(system_get_image_copy());
}

/**
 * Check the current header vs. the supplied Board ID
 *
 * @param type		Board ID type to check
 * @param inv		Board ID type (inverted)
 * @param flags		Board ID flags
 *
 * @return 0 if no mismatch, non-zero if mismatch
 */
static uint32_t check_board_id_vs_header(const struct board_id *id,
					 const struct SignedHeader *h)
{
	uint32_t mismatch = 0;

	/* Blank Board ID matches all headers */
	if (~(id->type & id->type_inv & id->flags) == 0)
		return 0;

	/*
	 * Masked bits in header BoardID type must match type and inverse from
	 * flash.
	 */
	mismatch = SIGNED_HEADER_PADDING ^ h->board_id_type ^ id->type;
	mismatch |= SIGNED_HEADER_PADDING ^ h->board_id_type ^ ~id->type_inv;
	mismatch &= SIGNED_HEADER_PADDING ^ h->board_id_type_mask;

	/*
	 * All 1-bits in header BoardID flags must be present in flags from
	 * flash
	 */
	mismatch |= (SIGNED_HEADER_PADDING ^ h->board_id_flags) & ~id->flags;

	return mismatch;
}

int read_board_id(struct board_id *id)
{
	int rv;

	/* Zero ID in case of failure */
	id->type = id->type_inv = id->flags = 0;

	rv = flash_physical_info_read_word(INFO_BOARD_ID_TYPE_OFFSET,
					   &id->type);
	if (rv)
		return rv;

	rv = flash_physical_info_read_word(INFO_BOARD_ID_TYPE_INV_OFFSET,
					   &id->type_inv);
	if (rv)
		return rv;

	return flash_physical_info_read_word(INFO_BOARD_ID_FLAGS_OFFSET,
					     &id->flags);
}

int write_board_id(const struct board_id *id)
{
	struct board_id id_test;
	uint32_t rv;

	/*
	 * Make sure the current header will still validate against the
	 * proposed values.  If it doesn't, then programming these values
	 * would cause the next boot to fail.
	 */
	if (check_board_id_vs_header(id, get_current_image_header()) != 0) {
		CPRINTS("%s: Board ID wouldn't allow current header", __func__);
		return EC_ERROR_INVAL;
	}

	/* Make sure INFO1 board ID space is readable */
	if (flash_info_read_enable(INFO_BOARD_ID_OFFSET,
				   INFO_BOARD_ID_PROTECT_SIZE) != EC_SUCCESS) {
		CPRINTS("%s: failed to enable read access to info", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	/* Fail if Board ID is already programmed */
	rv = read_board_id(&id_test);
	if (rv != EC_SUCCESS) {
		CPRINTS("%s: error reading Board ID", __func__);
		return rv;
	}
	if (~(id_test.type & id_test.type_inv & id_test.flags) != 0) {
		CPRINTS("%s: Board ID already programmed", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	/* Enable write access */
	if (flash_info_write_enable(INFO_BOARD_ID_OFFSET,
				    INFO_BOARD_ID_PROTECT_SIZE) != EC_SUCCESS) {
		CPRINTS("%s: failed to enable write access", __func__);
		return EC_ERROR_ACCESS_DENIED;
	}

	/* Write Board ID */
	rv = flash_info_physical_write(INFO_BOARD_ID_OFFSET,
				       sizeof(*id), (const char *)id);
	if (rv != EC_SUCCESS)
		CPRINTS("%s: write failed", __func__);

	/* Disable write access */
	flash_info_write_disable();

	return rv;
}

int check_board_id(void)
{
	struct board_id id;

	if (read_board_id(&id) != EC_SUCCESS)
		return EC_ERROR_UNKNOWN;

	/* Compare with header */
	if (check_board_id_vs_header(&id, get_current_image_header())
	    != EC_SUCCESS) {
		CPRINTS("Board ID check failed");
		return EC_ERROR_ACCESS_DENIED;
	}

	return EC_SUCCESS;
}

static enum vendor_cmd_rc vc_set_board_id(enum vendor_cmd_cc code,
					  void *buf,
					  size_t input_size,
					  size_t *response_size)
{
	struct board_id id;
	uint8_t *pbuf = buf;

	memcpy(&id.type, pbuf, sizeof(id.type));
	id.type_inv = ~id.type;
	memcpy(&id.flags, pbuf + sizeof(id.type), sizeof(id.flags));

	if (write_board_id(&id))
		return VENDOR_RC_WRITE_FLASH_FAIL;

	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_SET_BOARD_ID, vc_set_board_id);

static int command_set_board_id(int argc, char **argv)
{
	struct board_id id;
	char *e;

	if (argc != 3) {
		ccprintf("specify type and flags\n");
		return EC_ERROR_PARAM_COUNT;
	}

	id.type = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;
	id.type_inv = ~id.type;
	id.flags = strtoi(argv[2], &e, 0);
	if (*e)
		return EC_ERROR_PARAM2;

	return write_board_id(&id);
}
DECLARE_CONSOLE_COMMAND(bidset, command_set_board_id, NULL, "Set Board ID");

static enum vendor_cmd_rc vc_get_board_id(enum vendor_cmd_cc code,
					  void *buf,
					  size_t input_size,
					  size_t *response_size)
{
	struct board_id id;

	if (read_board_id(&id))
		return VENDOR_RC_READ_FLASH_FAIL;

	memcpy(buf, &id, sizeof(id));
	*response_size = sizeof(id);
	return VENDOR_RC_SUCCESS;
}
DECLARE_VENDOR_COMMAND(VENDOR_CC_GET_BOARD_ID, vc_get_board_id);
