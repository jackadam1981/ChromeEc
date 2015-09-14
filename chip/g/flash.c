/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "common.h"
#include "flash.h"
#include "registers.h"
#include "timer.h"

int flash_pre_init(void)
{
	return EC_SUCCESS;
}

int flash_physical_get_protect(int bank)
{
	return 0;				/* Not protected */
}

uint32_t flash_physical_get_protect_flags(void)
{
	return 0;				/* no flags set */
}

uint32_t flash_physical_get_valid_flags(void)
{
	/* These are the flags we're going to pay attention to */
	return EC_FLASH_PROTECT_RO_AT_BOOT |
		EC_FLASH_PROTECT_RO_NOW |
		EC_FLASH_PROTECT_ALL_NOW;
}

uint32_t flash_physical_get_writable_flags(uint32_t cur_flags)
{
	return 0;				/* no flags writable */
}

int flash_physical_protect_at_boot(enum flash_wp_range range)
{
	return EC_SUCCESS;			/* yeah, I did it. */
}

int flash_physical_protect_now(int all)
{
	return EC_SUCCESS;			/* yeah, I did it. */
}


enum flash_op {
	OP_ERASE_BLOCK,
	OP_WRITE_BLOCK,
};

static int do_flash_op(enum flash_op op, int byte_offset, int words)
{
	volatile uint32_t *fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL0);
	uint32_t opcode, tmp, errors;
	int i;
	int timedelay = 100;	     /* TODO(crosbug.com/p/45366): how long? */

	/* We have two flash banks. Adjust accordingly. */
	if (byte_offset >= CONFIG_FLASH_PHYSICAL_SIZE / 2) {
		byte_offset -= CONFIG_FLASH_PHYSICAL_SIZE / 2;
		fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL1);
	}

	/* What are we doing? */
	switch (op) {
	case OP_ERASE_BLOCK:
		opcode = 0x31415927;
		words = 0;			/* don't care, really */
		break;
	case OP_WRITE_BLOCK:
		opcode = 0x27182818;
		words--;		     /* count register is zero-based */
		break;
	}

	/* Set the parameters */
	GWRITE_FIELD(FLASH, FSH_TRANS, OFFSET,
		     byte_offset / 4);		/* word offset */
	GWRITE_FIELD(FLASH, FSH_TRANS, MAINB, 0);
	GWRITE_FIELD(FLASH, FSH_TRANS, SIZE, words);

	/* Kick it off */
	GWRITE(FLASH, FSH_PE_EN, 0xb11924e1);
	*fsh_pe_control = opcode;

	/* Wait for completion */
	for (i = 0; i < 50; i++) {
		/* TODO: No idea how long to wait here! */
		usleep(timedelay);
		tmp = *fsh_pe_control;
		if (tmp != opcode)
			break;
	}

	/* timed out waiting for control register to clear */
	if (tmp)
		return EC_ERROR_UNKNOWN;

	/* Check error status */
	errors = GREAD(FLASH, FSH_ERROR);

	/* Error status is self-clearing. Read it until it does (we hope). */
	for (i = 0; i < 50; i++) {
		usleep(timedelay);
		tmp = GREAD(FLASH, FSH_ERROR);
		if (!tmp)
			break;
	}

	if (errors || tmp)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

/* Write up to CONFIG_FLASH_WRITE_IDEAL_SIZE bytes at once */
static int write_batch(int byte_offset, int words, const uint8_t *data)
{
	volatile uint32_t *fsh_wr_data = GREG32_ADDR(FLASH, FSH_WR_DATA0);
	uint32_t val;
	int i;

	/* Load the write buffer. Treat data as a byte stream (big endian) */
	for (i = 0; i < words; i++) {
		/* CONFIG_FLASH_WRITE_SIZE is 4 for this SoC */
		val = ((data[0] << 24) | (data[1] << 16) |
		       (data[2] << 8) | data[3]);
		*fsh_wr_data = val;
		data += 4;
		fsh_wr_data++;
	}

	return do_flash_op(OP_WRITE_BLOCK, byte_offset, words);
}

int flash_physical_write(int byte_offset, int num_bytes, const char *data)
{
	int num, ret;

	/* The offset and size must be a multiple of CONFIG_FLASH_WRITE_SIZE */
	if (byte_offset % CONFIG_FLASH_WRITE_SIZE ||
	    num_bytes % CONFIG_FLASH_WRITE_SIZE)
		return EC_ERROR_INVAL;

	while (num_bytes) {
		num = MIN(num_bytes, CONFIG_FLASH_WRITE_IDEAL_SIZE);
		ret = write_batch(byte_offset,
				  num / 4,	/* word count */
				  (const uint8_t *)data);
		if (ret)
			return ret;

		num_bytes -= num;
		byte_offset += num;
		data += num;
	}

	return EC_SUCCESS;
}

int flash_physical_erase(int byte_offset, int num_bytes)
{
	int ret;

	/* Offset and size must be a multiple of CONFIG_FLASH_ERASE_SIZE */
	if (byte_offset % CONFIG_FLASH_ERASE_SIZE ||
	    num_bytes % CONFIG_FLASH_ERASE_SIZE)
		return EC_ERROR_INVAL;

	while (num_bytes) {
		/* We may be asked to erase multiple banks */
		ret = do_flash_op(OP_ERASE_BLOCK,
				  byte_offset,
				  num_bytes / 4); /* word count */
		if (ret)
			return ret;

		num_bytes -= CONFIG_FLASH_ERASE_SIZE;
		byte_offset += CONFIG_FLASH_ERASE_SIZE;
	}

	return EC_SUCCESS;
}
