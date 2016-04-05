/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Non-Volatile memory (NvMem) space is 16kB which is then divided into 8 -
 * 2kb blocks. Within CR-50 there are two customers for NvMem, 1) TPM2.0
 * specification and 2) CR-50 specific paramters such as BIOS password storage.
 *
 * TODO: Add design summary of A/B partition scheme.
 *
 */
#include <string.h>

#include "assert.h"
#include "common.h"
#include "console.h"
#include "dcrypto/dcrypto.h"
#include "flash.h"
#include "flash_config.h"
#include "nvmem_utils.h"
#include "shared_mem.h"
#include "timer.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)


/* Struct for NV block tag */
struct nvmem_tag {
	uint32_t sha;
	uint16_t version;
	uint16_t reserved;
};

/* NV Memory Block definitions */
#define NVMEM_START_ADDR (CONFIG_NV_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE)
#define NVMEM_NUM_PARTITIONS 2
#define NVMEM_PARTITION_SIZE (CONFIG_NV_MEM_SIZE / NVMEM_NUM_PARTITIONS)
#define NVMEM_BLOCK_SIZE CONFIG_FLASH_ERASE_SIZE
#define NVMEM_CR50_SIZE 0x400
#define NVMEM_TPM_SIZE (NVMEM_PARTITION_SIZE - NVMEM_CR50_SIZE -\
			sizeof(struct nvmem_tag))
#define NVMEM_NUM_BLOCKS (NVMEM_PARTITION_SIZE / NVMEM_BLOCK_SIZE)

#define NVMEM_ACQUIRE_CACHE_SLEEP_MS 20
#define NVMEM_ACQUIRE_CACHE_MAX_ATTEMPTS (200 / NVMEM_ACQUIRE_CACHE_SLEEP_MS)
#define NVMEM_CACHE_ALIGN_BITS 4


/* Structure for physical NvMem block */
struct nvmem_partition {
	struct nvmem_tag tag;
	uint8_t tpm_data[NVMEM_TPM_SIZE];
	uint8_t cr50_data[NVMEM_CR50_SIZE];
};

/* A/B partion that is most up to date */
static int nvmem_act_partion;

/* NvMem Cache Memory pointer */
static uint8_t *cache_base_ptr;

static int nvmem_acquire_cache(void)
{
	int attempts = 0;
	int avail_cache_size;
	int ret;
	int align_offset;
	int addr;

	avail_cache_size = shared_mem_size();
	align_offset = (1 << NVMEM_CACHE_ALIGN_BITS) - 1;
	if (avail_cache_size < NVMEM_PARTITION_SIZE + align_offset) {
		CPRINTF("Not enough cache! avail = 0x%x, part = 0x%x\n",
			avail_cache_size, NVMEM_PARTITION_SIZE);
		return EC_ERROR_OVERFLOW;
	}

	while (attempts < NVMEM_ACQUIRE_CACHE_MAX_ATTEMPTS) {
		ret = shared_mem_acquire(NVMEM_PARTITION_SIZE,
					 (char **)&cache_base_ptr);
		if (ret == EC_SUCCESS) {
			/* Align the start address */
			addr = (int)cache_base_ptr;
			addr = ((addr + align_offset) >>
				NVMEM_CACHE_ALIGN_BITS) <<
				NVMEM_CACHE_ALIGN_BITS;
			cache_base_ptr = (uint8_t *)addr;
			return EC_SUCCESS;
		} else if (ret == EC_ERROR_BUSY)
			/* wait NVMEM_ACQUIRE_CACHE_SLEEP_MS  msec */
			/* TODO: what time really makes sense?? */
			msleep(NVMEM_ACQUIRE_CACHE_SLEEP_MS);
		attempts++;
	}
	return EC_ERROR_TIMEOUT;
}

static void nvmem_release_cache(void)
{
	shared_mem_release(cache_base_ptr);
	cache_base_ptr = NULL;
}

int nvmem_setup(void)
{
	int ret;

	ret = nvmem_acquire_cache();
	if (ret != EC_SUCCESS) {
		CPRINTF("NvMem: Cache ram not available!\n");
		return ret;
	}
	nvmem_release_cache();
	return EC_SUCCESS;
}

static int nvmem_find_partition(void)
{
	/* stub for now, just assume it's partiion 0 */
	nvmem_act_partion = 0;

	return EC_SUCCESS;
}

int nvmem_init(void)
{
	cache_base_ptr = NULL;
	nvmem_find_partition();


	return EC_SUCCESS;
}

void nvmem_read(unsigned int offset, unsigned int size,
		    void *data, enum nvmem_users user)
{
	struct nvmem_partition *p_part;
	int max_offset;
	uint8_t *p_src;

	/* Point to either NvMem flash or ram if that's active */
	if (cache_base_ptr == NULL)
		p_part = (struct nvmem_partition *)(NVMEM_START_ADDR +
						     nvmem_act_partion *
						     NVMEM_PARTITION_SIZE);
	else
		p_part = (struct nvmem_partition *)cache_base_ptr;

	/* Point to the required data buffer based on user */
	if (user == NV_TPM) {
		p_src = p_part->tpm_data;
		max_offset = NVMEM_TPM_SIZE;
	} else if (user == NV_CR50) {
		p_src = p_part->cr50_data;
		max_offset = NVMEM_CR50_SIZE;
	} else {
		/* TODO: Shouldn't be invalid, what do with this case? */
		CPRINTF("Invalid NvMem user entry: %d\n", user);
		return;
	}
	/* Advance to the correct byte within the data buffer */
	p_src += offset;

	assert(offset + size < max_offset);
	/* Copy from src into the caller's destination buffer */
	memcpy(data, p_src, size);
}

static int nvmem_wr_acquire_cache(void)
{
	uint8_t *p_src;

	/*
	 * If cache_base_ptr is NULL, then nothing to do. However, if NULL, then
         * need to first acquire the shared memory buffer. Then, the full
         * partition needs to be copied from flash into the cache buffer.
	 */
	/* TODO(crbug.com/52520): Make this reentrant to prevent a 2nd task from
	 * calling and releasing.
	 */
	if (cache_base_ptr == NULL) {
		if (nvmem_acquire_cache() != EC_SUCCESS)
			return EC_ERROR_TIMEOUT;
		/* Copy partiion contents from flash into cache buffer */
		p_src = (uint8_t *)(NVMEM_START_ADDR + nvmem_act_partion *
				    NVMEM_PARTITION_SIZE);
		memcpy(cache_base_ptr, p_src, NVMEM_PARTITION_SIZE);
	}

	return EC_SUCCESS;
}

void nvmem_write(unsigned int offset, unsigned int size,
		 void *data, enum nvmem_users user)
{
	struct nvmem_partition *p_part;
	uint8_t *p_dest;
	int max_offset;

	/* Make sure that the cache buffer is active */
	nvmem_wr_acquire_cache();
	/* Overlay partition at start of cache buffer. */
	p_part = (struct nvmem_partition *)cache_base_ptr;
	/* Select correct desitination buffer based on user */
	if (user == NV_TPM) {
		p_dest = p_part->tpm_data;
		max_offset = NVMEM_TPM_SIZE;
	} else if (user == NV_CR50) {
		p_dest = p_part->cr50_data;
		max_offset = NVMEM_CR50_SIZE;
	} else {
		/* TODO: What should happen in this unexpected case? */
		CPRINTF("Invalid NvMem user entry: %d\n", user);
		return;
	}
	/* Advance to correct offset within data buffer */
	p_dest += offset;

	assert(offset + size < max_offset);
	/* Copy data from caller into destination buffer */
	memcpy(p_dest, data, size);
}

int nvmem_commit(void)
{
	int block;
	int cache_offset;
	int nvmem_offset;

	/* Point to first block within active partition */
	cache_offset = 0;
	nvmem_offset = CONFIG_NV_MEM_OFF + nvmem_act_partion *
			NVMEM_PARTITION_SIZE;
	/* Write partition to NvMem */
	for (block = 0; block < NVMEM_NUM_BLOCKS; block++) {
		/* Erase block */
		if (flash_physical_erase(nvmem_offset,
					 NVMEM_BLOCK_SIZE)) {
			CPRINTF("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_UNKNOWN;
		}
		/* Write block */
		if (flash_physical_write(nvmem_offset,
					 NVMEM_BLOCK_SIZE,
					 &cache_base_ptr[cache_offset])) {
			CPRINTF("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_UNKNOWN;
		}
		cache_offset += NVMEM_BLOCK_SIZE;
		nvmem_offset += NVMEM_BLOCK_SIZE;
	}
	/* Free up scratch buffers */
	nvmem_release_cache();

	return EC_SUCCESS;
}
