/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Non-Volatile memory (NvMem) space is 16kB which is then divided into 8 -
 * 2kb blocks. 2kB is the minimum size that can be erased. Writes can be done
 * in as little as 4 byte pieces, but that presumes the location has already
 * been erased. Within CR-50 there are two customers for NvMem, 1) TPM2.0
 * specification and 2) CR-50 specific paramters such as BIOS password storage.
 *
 * In order to provide maximum robustness for NvMem operations, the NvMem space
 * is divided into two equal sized partions (where each partition consist of 4
 * blocks = 8kB). The partitions contain 3 elements.
 *
 *     1. Tag
 *     2. Data buffer for TPM2.0 speicification
 *     3. Data buffer for Cr-50 needs
 *
 *     NvMem Partiion
 *     ---------------------------------------------------------------------
 *     |0x8 tag |        0x17F8 bytes TPM2.0          |  0x400 bytes Cr-50 |
 *     ---------------------------------------------------------------------
 *
 *     Physical Block Tag details
 *     ---------------------------------------------------------------------
 *     |             sha               |    version      |    reserved     |
 *     ---------------------------------------------------------------------
 *         sha       -> 4 bytes of sha1 digest
 *         version   -> 2 bytes version number (0 - 0xfffe)
 *         reserved  -> 2 bytes
 *
 * At initialization time, each partition is scanned to see if it has a good sha
 * entry. One of the two partitions being valid is a supported condition. If
 * however, neither partiion is valid, then a check is made to see if NvMem
 * space is fully erased. If this is detected then the tag for partion 0 is
 * popuplated and written into flash. If neither partition is valid and they
 * aren't fully erased, then NvMem is marked corrupt and this failure condition
 * must be reported back up the TPM2.0 stack.
 *
 * The version number is used to distinguish between two valid partitions with
 * the newsest version number (in a ciruclar sense) marking the correct
 * partition to use. The parition number 0/1 is tracked via a static
 * variable. When the NvMem contents need to be updated the flash erase/write of
 * the updated partition will use the inactive partition space in NvMem. This
 * way if there is a critical failure (i.e. loss of power) during the erase or
 * write operation, then the contents of the active partition prior the most
 * recent writes will sill preserved.
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

#define NVMEM_SHA_SIZE 4
#define NVMEM_VERSION_BITS 16
#define NVMEM_VERSION_MASK ((1 << NVMEM_VERSION_BITS) - 1)
/* Struct for NV block tag */
struct nvmem_tag {
	uint8_t sha[NVMEM_SHA_SIZE];
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
#define NVMEM_NOT_INITIALIZED (-1)

/* Structure for physical NvMem block */
struct nvmem_partition {
	struct nvmem_tag tag;
	uint8_t tpm_data[NVMEM_TPM_SIZE];
	uint8_t cr50_data[NVMEM_CR50_SIZE];
};

/* A/B partion that is most up to date */
static int nvmem_act_partition;
/* NvMem Cache Memory pointer */
static uint8_t *cache_base_ptr;

static void nvmem_compute_sha(uint8_t *p_buf, int num_bytes, uint8_t *p_sha)
{
	uint8_t sha1_digest[SHA1_DIGEST_SIZE];
	/*
	 * Taking advantage of the built in dcrypto engine to generate
	 * a CRC-like value that can be used to validate contents of an
	 * NvMem partition. Only using the lower 4 bytes of the sha1 hash.
	 */
	DCRYPTO_SHA1_hash((uint8_t *)p_buf,
			  num_bytes,
			  sha1_digest);
	memcpy(p_sha, sha1_digest, NVMEM_SHA_SIZE);
}

static int nvmem_verify_partition_sha(int index)
{
	uint8_t sha_comp[NVMEM_SHA_SIZE];
	struct nvmem_partition *p_part;
	uint8_t *p_data;

	p_part = (struct nvmem_partition *)NVMEM_START_ADDR;
	p_part += index;
	p_data = (uint8_t *)p_part;
	p_data += NVMEM_SHA_SIZE;

	/* Number of bytes to compute sha over */
	nvmem_compute_sha(p_data,
			  (NVMEM_PARTITION_SIZE - NVMEM_SHA_SIZE),
			  sha_comp);
	/* Check if computed value matches stored value. */
	return memcmp(p_part->tag.sha, sha_comp, NVMEM_SHA_SIZE);
}

static void nvmem_cache_init(void)
{
	cache_base_ptr = NULL;
}

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
		CPRINTF("Not enough shared mem! avail = 0x%x < reqd = 0x%x\n",
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
			/* TODO: what time really makes sense? */
			msleep(NVMEM_ACQUIRE_CACHE_SLEEP_MS);
		attempts++;
	}

	return EC_ERROR_TIMEOUT;
}

static int nvmem_update_cache_ptr(void)
{
	uint8_t *p_src;

	/*
	 * If cache_base_ptr is NULL, then nothing to do. However, if NULL, then
	 * need to first acquire the shared memory buffer. Then, the full
	 * partition needs to be copied from flash into the cache buffer.
	 */
	if (cache_base_ptr == NULL) {
		if (nvmem_acquire_cache() != EC_SUCCESS)
			return EC_ERROR_TIMEOUT;
		/* Copy partiion contents from flash into cache buffer */
		p_src = (uint8_t *)(NVMEM_START_ADDR + nvmem_act_partition *
				    NVMEM_PARTITION_SIZE);
		memcpy(cache_base_ptr, p_src, NVMEM_PARTITION_SIZE);
	}

	return EC_SUCCESS;
}

static void nvmem_release_cache(void)
{
	/* Done with shared memory buffer, release it. */
	shared_mem_release(cache_base_ptr);
	/* Inidicate cache is not available */
	cache_base_ptr = NULL;
}

static int nvmem_is_unitialized(void)
{
	int n;
	int ret;
	uint32_t *p_nvmem;
	struct nvmem_partition *p_part;

	/* Point to start of Nv Memory */
	p_nvmem = (uint32_t *)NVMEM_START_ADDR;
	/* Verify that each byte is 0xff (4 bytes at a time) */
	for (n = 0; n < (CONFIG_NV_MEM_SIZE >> 2); n++) {
		if (p_nvmem[n] != 0xffffffff)
			return EC_ERROR_CRC;
	}

	/*
	 * NvMem is fully unitialized. Need to initialize tag and write tag to
	 * flash so at least 1 partition is ready to be used.
	 */
	CPRINTF("NvMem: Fully erased, Setting up partition 0\n");
	nvmem_act_partition = 0;
	/* Need to acquire the shared memory buffer */
	ret = nvmem_update_cache_ptr();
	if (ret != EC_SUCCESS)
		return ret;
	p_part = (struct nvmem_partition *)cache_base_ptr;
	/* Start with version 0 */
	p_part->tag.version = 0;
	/* Compute sha with updated tag */
	nvmem_compute_sha(&cache_base_ptr[NVMEM_SHA_SIZE],
			  NVMEM_PARTITION_SIZE - NVMEM_SHA_SIZE,
			  p_part->tag.sha);
	/*
	 * Partition 0 is initialized, write tag only to flash. Since the
	 * partition was just verified to be fully erased, can just do write
	 * operation.
	 */
	if (flash_physical_write(CONFIG_NV_MEM_OFF,
				 sizeof(struct nvmem_tag),
				 cache_base_ptr)) {
		CPRINTF("%s:%d\n", __func__, __LINE__);
		nvmem_release_cache();
		return EC_ERROR_UNKNOWN;
	}
	/* Can release the cache buffer now */
	nvmem_release_cache();
	return EC_SUCCESS;
}

static int nvmem_compare_version(void)
{
	struct nvmem_partition *p_part;
	uint16_t ver0, ver1;
	uint32_t delta;

	p_part = (struct nvmem_partition *)NVMEM_START_ADDR;
	ver0 = p_part->tag.version;
	p_part++;
	ver1 = p_part->tag.version;

	/* Compute version difference accounting for wrap condition */
	delta = (ver0 - ver1 + (1<<NVMEM_VERSION_BITS)) & NVMEM_VERSION_MASK;
	/*
	 * If version number delta is positive in a circular sense then
	 * partition 0 has the newest version number. Otherwise, it's
	 * partition 1.
	 */
	return delta < (1<<(NVMEM_VERSION_BITS-1)) ? 0 : 1;
}

static int nvmem_find_partition(void)
{
	int partition;

	/* Don't know which partition to use yet */
	nvmem_act_partition = NVMEM_NOT_INITIALIZED;
	/*
	 * Check each partition to determine if the sha is good. If both
	 * partitions have valid sha(s), then compare version numbers to select
	 * the most recent one.
	 */
	for (partition = 0; partition < NVMEM_NUM_PARTITIONS; partition++)
		if (nvmem_verify_partition_sha(partition) == EC_SUCCESS) {
			if (nvmem_act_partition == NVMEM_NOT_INITIALIZED)
				nvmem_act_partition = partition;
			else
				nvmem_act_partition = nvmem_compare_version();
		}
	/*
	 * If active_partition is still not selected, then neither partition is
	 * valid. In this case need to determine if they are simply erased or
	 * both are corrupt. If erased, then can initialze the tag for the first
	 * one. If not fully erased, then this is an error condition.
	 */
	if (nvmem_act_partition != NVMEM_NOT_INITIALIZED) {
		CPRINTF("NvMem: Starting with partition %d\n",
			nvmem_act_partition);
		return EC_SUCCESS;
	}

	if (nvmem_is_unitialized()) {
		CPRINTF("NvMem: No Valid Paritions and not fully erased!!\n");
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

int nvmem_setup(uint16_t starting_version)
{
	struct nvmem_partition *p_part;
	int partition;

	CPRINTF("Configuring NVMEM Partitions: starting ver = %d\n",
		starting_version);
	/*
	 * Initialize NVmem partition. This function will only be called
	 * if during nvmem_init() a valid translation table can't be
	 * built. A one to one mapping is assumed, and all block version
	 * numbers are reset to 0.
	 */
	for (partition = 0; partition < NVMEM_NUM_PARTITIONS; partition++) {
		/* Set active partition variable */
		nvmem_act_partition = partition;
		/* Get the cache buffer */
		if (nvmem_update_cache_ptr() != EC_SUCCESS) {
			CPRINTF("NvMem: Cache ram not available!\n");
			return EC_ERROR_TIMEOUT;
		}
		/* Fill in tag info */
		p_part = (struct nvmem_partition *)cache_base_ptr;
		p_part->tag.version = starting_version + partition;
		nvmem_compute_sha(&cache_base_ptr[NVMEM_SHA_SIZE],
				  NVMEM_PARTITION_SIZE - NVMEM_SHA_SIZE,
				  p_part->tag.sha);
		/* Partition is now ready, write it to flash. */
		nvmem_commit();
	}

	return EC_SUCCESS;
}


int nvmem_init(void)
{
	int ret;

	nvmem_cache_init();
	/* Verify at least one good partition is available */
	ret = nvmem_find_partition();
	if (ret != EC_SUCCESS) {
		CPRINTF("NvMem init faile, partitions are corrupted\n");
		return ret;
	}

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
						     nvmem_act_partition *
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

void nvmem_write(unsigned int offset, unsigned int size,
		 void *data, enum nvmem_users user)
{
	struct nvmem_partition *p_part;
	uint8_t *p_dest;
	int max_offset;

	/* Make sure that the cache buffer is active */
	if (nvmem_update_cache_ptr())
		/* TODO: What to do when can't access cache buffer? */
		return;
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
	int new_active_partition;
	uint16_t version;
	struct nvmem_partition *p_part;

	/* Update version number */
	p_part = (struct nvmem_partition *)cache_base_ptr;
	version = p_part->tag.version + 1;
	/* Check for restricted version number */
	if (version == NVMEM_VERSION_MASK)
		version = 0;
	p_part->tag.version = version;
	/* Update the sha */
	nvmem_compute_sha(&cache_base_ptr[NVMEM_SHA_SIZE],
			  NVMEM_PARTITION_SIZE - NVMEM_SHA_SIZE,
			  p_part->tag.sha);

	/* Toggle parition being used (always write to current spare) */
	new_active_partition = nvmem_act_partition ^ 1;
	/* Point to first block within active partition */
	cache_offset = 0;
	nvmem_offset = CONFIG_NV_MEM_OFF + new_active_partition *
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
	/* Update newest partition index */
	nvmem_act_partition = new_active_partition;
	return EC_SUCCESS;
}
