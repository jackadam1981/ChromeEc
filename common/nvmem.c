/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Non-volatile memory
 */

#include "common.h"
#include "console.h"
#include "flash.h"
#include "nvmem.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, "NVM " format, ## args)

#define NVMEM_ERR_CORRUPT	-1
#define NVMEM_ERR_INVALID	-2
#define NVMEM_ERR_READ		-3

const int nvmem_block_size = NVMEM_BLOCK_SIZE;
const int nvmem_flash_offset = NVMEM_FLASH_OFF;
static int nvmem_offset;

static struct datablob_nvmem {
	const struct datablob_driver *driver;
	const int record_size;
	int cache_status;
	uint8_t cache[NVMEM_RECORD_SIZE];
} nvmem;

static int nvmem_is_free(uint8_t *record, int size)
{
	while (size) {
		if (record[--size] != 0xff)
			return 0;
	}
	return 1;
}

/*
 * Read the entire block and return the offset after the last valid record if
 * the block looks good. Otherwise, it returns the following:
 *
 * - ERR_INVALID: Invalid record found.
 * - ERR_CORRUPT: Found valid record after free slot.
 * - ERR_READ:    Failed to read.
 */
static int _flash_scan(void)
{
	struct datablob_header *h = (struct datablob_header *)nvmem.cache;
	int offset;
	int found_record = 0;
	int found_free = 0;
	int rv;

	for (offset = nvmem_flash_offset;
			offset < nvmem_flash_offset + nvmem_block_size;
			offset += nvmem.record_size) {
		rv = flash_read(offset, nvmem.record_size, nvmem.cache);
		if (rv) {
			CPRINTS("NVMEM: Failed to read (0x%x)", rv);
			return NVMEM_ERR_READ;
		}

		if (memcmp(h->magic, datablob_magic, sizeof(h->magic))
				|| (h->major_version > DATABLOB_VERSION_MAJOR)
				|| (h->total_size < sizeof(*h))
				|| (nvmem.record_size < h->total_size)
				|| (datablob_crc8(h) != h->crc)) {
			if (nvmem_is_free(nvmem.cache, nvmem.record_size)) {
				found_free = 1;
			} else {
				CPRINTS("NVMEM: Found invalid record");
				return NVMEM_ERR_INVALID;
			}
		} else {
			if (found_free) {
				CPRINTS("NVMEM: Block looks corrupted");
				return NVMEM_ERR_CORRUPT;
			}
			found_record = 1;
		}
	}

	return found_record ? offset : nvmem_flash_offset;
}

static int _flash_erase(void)
{
	int rv;

	CPRINTS("NVMEM: Formatting...");
	rv = flash_erase(nvmem_flash_offset, nvmem_block_size);
	if (rv) {
		nvmem_offset = 0;
		CPRINTS("NVMEM: Failed to format (0x%x)", rv);
		return EC_ERROR_UNKNOWN;
	}

	/* Set pointer to the beginning of the block. */
	nvmem_offset = nvmem_flash_offset;
	return EC_SUCCESS;
}

static int _flash_is_protected(void)
{
	/* NVMEM resides in RW, which is always writable. */
	return 0;
}

static int _flash_write(const uint8_t *record, int record_size)
{
	if (!nvmem_offset) {
		int offset;

		CPRINTS("NVMEM: Initializing");
		offset = _flash_scan();
		if (offset == NVMEM_ERR_READ) {
			return EC_ERROR_UNKNOWN;
		} else if (offset == NVMEM_ERR_CORRUPT ||
				offset == NVMEM_ERR_INVALID) {
			/* No records found */
			datablob_create(&nvmem);
			if (_flash_erase())
				return EC_ERROR_UNKNOWN;
		}
	}

	if (nvmem_offset == nvmem_flash_offset + nvmem_block_size) {
		/* All used */
		if (_flash_erase())
			return EC_ERROR_UNKNOWN;
	}

	if (flash_write(nvmem_offset, record_size, record)) {
		CPRINTS("NVMEM: Failed to write");
		return EC_ERROR_UNKNOWN;
	}

	nvmem_offset += record_size;

	return EC_SUCCESS;
}

static int _flash_read(uint8_t *record, int record_size)
{
	int offset = _flash_scan();

	if (offset == NVMEM_ERR_READ) {
		return EC_ERROR_UNKNOWN;
	} else if (offset == NVMEM_ERR_CORRUPT
			|| offset == NVMEM_ERR_INVALID) {
		return EC_ERROR_INVAL;
	} else if (offset == nvmem_flash_offset) {
		/* All slots are free */
		CPRINTS("NVMEM: All free");
		datablob_create(&nvmem);
	}
	nvmem_offset = offset;

	return EC_SUCCESS;
}

/*
 * NVMEM APIs
 */
int nvmem_get_offset(void)
{
	return nvmem_offset;
}

int nvmem_get_cache_status(void)
{
	return nvmem.cache_status;
}

int nvmem_create(void)
{
	datablob_create(&nvmem);
	return EC_SUCCESS;
}

int nvmem_get(int tag, uint8_t *buf, uint8_t *size)
{
	return datablob_get_data(&nvmem, tag, buf, size);
}

int nvmem_set(int tag, const uint8_t *buf, uint8_t size)
{
	return datablob_set_data(&nvmem, tag, buf, size);
}

int nvmem_write(void)
{
	return datablob_write(&nvmem);
}

void nvmem_reset(void)
{
	uint8_t free_record[NVMEM_RECORD_SIZE];
	int p;

	memset(free_record, 0xff, nvmem.record_size);

	for (p = nvmem_flash_offset; p < nvmem_flash_offset + nvmem_block_size;
			p += nvmem.record_size)
		flash_write(p, sizeof(free_record), free_record);

	memset(&nvmem.cache, 0, sizeof(nvmem.cache));
	nvmem.cache_status = DATABLOB_CACHE_INVALID;
}

const struct datablob_driver flash_drv = {
	.save = _flash_write,
	.load = _flash_read,
	.erase = _flash_erase,
	.is_protected = _flash_is_protected,
};

static struct datablob_nvmem nvmem = {
	.driver = &flash_drv,
	.record_size = NVMEM_RECORD_SIZE,
};
