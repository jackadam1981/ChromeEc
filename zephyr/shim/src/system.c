/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_version.h"
#include "system.h"

#define IS_BIT_SET(reg, bit) (((reg) >> (bit)) & (0x1))

/* Return true if index is stored as a single byte in bbram */
static int bbram_is_byte_access(enum bbram_data_index index)
{
	return (index >= BBRM_DATA_INDEX_VBNVCNTXT &&
		index <  BBRM_DATA_INDEX_RAMLOG)
		|| index == BBRM_DATA_INDEX_PD0
		|| index == BBRM_DATA_INDEX_PD1
		|| index == BBRM_DATA_INDEX_PD2
		|| index == BBRM_DATA_INDEX_PANIC_FLAGS
	;
}

/* Check index is within valid BBRAM range and IBBR is not set */
static int bbram_valid(enum bbram_data_index index, int bytes)
{
	/* Check index */
	if (index < 0 || index + bytes > BBRAM_SIZE)
		return 0;

	/* Check BBRAM is valid */
	if (IS_BIT_SET(BBRAM_BKUP_STS, NPCX_BKUP_STS_IBBR)) {
		NPCX_BKUP_STS = BIT(NPCX_BKUP_STS_IBBR);
		panic_printf("IBBR set: BBRAM corrupted!\n");
		return 0;
	}
	return 1;
}

/**
 * Read battery-backed ram (BBRAM) at specified index.
 *
 * @return The value of the register or 0 if invalid index.
 */
static uint32_t bbram_data_read(enum bbram_data_index index)
{
	uint32_t value = 0;
	int bytes = bbram_is_byte_access(index) ? 1 : 4;

	if (!bbram_valid(index, bytes))
		return 0;

	/* Read BBRAM */
	if (bytes == 4) {
		value += BBRAM(index + 3);
		value = value << 8;
		value += BBRAM(index + 2);
		value = value << 8;
		value += BBRAM(index + 1);
		value = value << 8;
	}
	value += NPCX_BBRAM(index);

	return value;
}

/* Map idx to a returned BBRM_DATA_INDEX_*, or return -1 on invalid idx */
static int bbram_idx_lookup(enum system_bbram_idx idx)
{
	if (idx >= SYSTEM_BBRAM_IDX_VBNVBLOCK0 &&
	    idx <= SYSTEM_BBRAM_IDX_VBNVBLOCK15)
		return BBRM_DATA_INDEX_VBNVCNTXT +
		       idx - SYSTEM_BBRAM_IDX_VBNVBLOCK0;
	if (idx == SYSTEM_BBRAM_IDX_PD0)
		return BBRM_DATA_INDEX_PD0;
	if (idx == SYSTEM_BBRAM_IDX_PD1)
		return BBRM_DATA_INDEX_PD1;
	if (idx == SYSTEM_BBRAM_IDX_PD2)
		return BBRM_DATA_INDEX_PD2;
	if (idx == SYSTEM_BBRAM_IDX_TRY_SLOT)
		return BBRM_DATA_INDEX_TRY_SLOT;
	return -1;
}

int system_get_bbram(enum system_bbram_idx idx, uint8_t *value)
{
	int bbram_idx = bbram_idx_lookup(idx);

	if (bbram_idx < 0)
		return EC_ERROR_INVAL;

	*value = bbram_data_read(bbram_idx);
	return EC_SUCCESS;
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
}

const char *system_get_chip_vendor(void)
{
	return "chromeos";
}

const char *system_get_chip_name(void)
{
	return "emu";
}

const char *system_get_chip_revision(void)
{
	return "";
}

const void *__image_size;
