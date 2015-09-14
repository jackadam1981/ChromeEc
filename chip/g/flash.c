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


/* Write up to CONFIG_FLASH_WRITE_IDEAL_SIZE bytes at once */
static int write_batch(int offset, int bytes, const char *data)
{
	uint32_t val, tmp1, tmp2;
	volatile uint32_t *fsh_wr_data, *fsh_pe_control;
	int i, words, m_offset;

	/* We have two flash macros. Which one are we poking? */
	if (offset >= CONFIG_FLASH_PHYSICAL_SIZE / 2) {
		m_offset = offset - CONFIG_FLASH_PHYSICAL_SIZE / 2;
		fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL1);
	} else {
		m_offset = offset;
		fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL0);
	}


	/*
	 * TRANS_OFFSET = word address, not byte address
	 * TRANS_MAINB = 0
	 */
	GWRITE_FIELD(FLASH, FSH_TRANS, OFFSET, m_offset / 4);
	GWRITE_FIELD(FLASH, FSH_TRANS, MAINB, 0);

	words = bytes / 4;

	fsh_wr_data = GREG32_ADDR(FLASH, FSH_WR_DATA0);
	for (i = 0; i < words; i++) {
		/* byte stream == big endian */
		val = ((data[0] << 24) | (data[1] << 16) |
		       (data[2] << 8) | data[3]);
		*fsh_wr_data = val;
		data += 4;
		fsh_wr_data++;
	}

	/* TRANS_SIZE = num words - 1 (we always write at least one)*/
	GWRITE_FIELD(FLASH, FSH_TRANS, SIZE, words - 1);

	GWRITE(FLASH, FSH_PE_EN, 0xb11924e1);
	*fsh_pe_control = 0x27182818;

	for (i = 0; i < 50; i++) {
		usleep(100);
		tmp1 = *fsh_pe_control;
		if (tmp1 != 0x27182818)
			break;
	}

	for (i = 0; i < 50; i++) {
		usleep(100);
		tmp2 = GREAD(FLASH, FSH_ERROR);
		if (!tmp2)
			break;
	}

	if (tmp1 || tmp2)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

int flash_physical_write(int offset, int size, const char *data)
{
	int num, ret;

	/* Don't try to handle partial word writes */
	if (size % CONFIG_FLASH_WRITE_SIZE)
		return EC_ERROR_INVAL;

	while (size) {
		num = MIN(size, CONFIG_FLASH_WRITE_IDEAL_SIZE);
		ret = write_batch(offset, num, data);
		if (ret)
			return ret;

		size -= num;
		offset += num;
		data += num;
	}

	return EC_SUCCESS;
}

static int erase_batch(int offset, int size)
{
	uint32_t tmp1, tmp2, i;
	int m_offset;
	volatile uint32_t *fsh_pe_control;

	/* We have two flash macros. Which one are we poking? */
	if (offset >= CONFIG_FLASH_PHYSICAL_SIZE / 2) {
		m_offset = offset - CONFIG_FLASH_PHYSICAL_SIZE / 2;
		fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL1);
	} else {
		m_offset = offset;
		fsh_pe_control = GREG32_ADDR(FLASH, FSH_PE_CONTROL0);
	}

	/*
	 * TRANS_OFFSET = word address, not byte address
	 * TRANS_MAINB = 0
	 * TRANS_SIZE = xx
	 */
	GWRITE_FIELD(FLASH, FSH_TRANS, OFFSET, m_offset / 4);
	GWRITE_FIELD(FLASH, FSH_TRANS, MAINB, 0);

	GWRITE(FLASH, FSH_PE_EN, 0xb11924e1);
	*fsh_pe_control = 0x31415927;

	for (i = 0; i < 50; i++) {
		usleep(100);
		tmp1 = *fsh_pe_control;

		if (tmp1 != 0x31415927)
			break;
	}

	for (i = 0; i < 50; i++) {
		usleep(100);
		tmp2 = GREAD(FLASH, FSH_ERROR);
		if (!tmp2)
			break;
	}

	if (tmp1 || tmp2)
		return EC_ERROR_UNKNOWN;

	return EC_SUCCESS;
}

/* Offset and size must be a multiple of CONFIG_FLASH_ERASE_SIZE */
int flash_physical_erase(int offset, int size)
{
	int ret;

	while (size) {
		ret = erase_batch(offset, size);
		if (ret)
			return ret;

		size -= CONFIG_FLASH_ERASE_SIZE;
		offset += CONFIG_FLASH_ERASE_SIZE;
	}

	return EC_SUCCESS;
}
