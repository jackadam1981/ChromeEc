/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * In order to provide maximum robustness for NvMem operations, the NvMem space
 * is divided into two equal sized partitions. A partition contains a tag
 * and a buffer for each NvMem user.
 *
 *     NvMem Partiion
 *     ---------------------------------------------------------------------
 *     |0x8 tag | User Buffer 0 | User Buffer 1 | .... |  User Buffer N-1  |
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

#include "assert.h"
#include "common.h"
#include "console.h"
#include "flash.h"
#include "nvmem.h"
#include "shared_mem.h"
#include "timer.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

#define NVMEM_VERSION_BITS 16
#define NVMEM_VERSION_MASK ((1 << NVMEM_VERSION_BITS) - 1)


#define NVMEM_ACQUIRE_CACHE_SLEEP_MS 20
#define NVMEM_ACQUIRE_CACHE_MAX_ATTEMPTS (200 / NVMEM_ACQUIRE_CACHE_SLEEP_MS)
#define NVMEM_CACHE_ALIGN_BITS 4
#define NVMEM_NOT_INITIALIZED (-1)

/* Structure MvMem Partition */
struct nvmem_partition {
	struct nvmem_tag tag;
	uint8_t buffer[NVMEM_PARTITION_SIZE - sizeof(struct nvmem_tag)];
};
/* Pointer to NvMem Buffer length table */
static int32_t *p_buffer_tab;
static int32_t nvmem_num_buffers;

/* A/B partion that is most up to date */
static int nvmem_act_partition;
/* NvMem Cache Memory pointer */
static uint8_t *cache_base_ptr;


static int nvmem_verify_partition_sha(int index)
{
	uint8_t sha_comp[NVMEM_SHA_SIZE];
	struct nvmem_partition *p_part;
	uint8_t *p_data;

	p_part = (struct nvmem_partition *)NVMEM_BASE_ADDR;
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

	/* TODO(crbug.com/52520): Make this reentrant to prevent a 2nd task from
	 * calling and releasing.
	 */

	if (cache_base_ptr == NULL) {
		if (nvmem_acquire_cache() != EC_SUCCESS)
			return EC_ERROR_TIMEOUT;
		/* Copy partiion contents from flash into cache buffer */
		p_src = (uint8_t *)(NVMEM_BASE_ADDR + nvmem_act_partition *
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
	p_nvmem = (uint32_t *)NVMEM_BASE_ADDR;
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

	p_part = (struct nvmem_partition *)NVMEM_BASE_ADDR;
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

static int nvmem_get_partition_off(int user, uint32_t offset,
				   uint32_t len, int32_t *p_buf_offset)
{
	int32_t max_len;
	int32_t start_offset;
	int32_t buffer_offset;
	int n;

	/* Sanity check for 'user' and table being initialized */
	if (user >= nvmem_num_buffers)
		return EC_ERROR_OVERFLOW;
	if (p_buffer_tab == NULL)
		return EC_ERROR_UNKNOWN;

	/*
	 * NvMem user buffers are continguous in a partition. Determine the
	 * starting offset for the user by adding the length of user buffers
	 * which have lower user numbers than the current user.
	 */
	start_offset = 0;
	buffer_offset = 0;
	for (n = 0; n <= user; n++) {
		start_offset += buffer_offset;
		/* Buffer length for user n */
		max_len = p_buffer_tab[n];
		/* Add buffer length for current user for start of next */
		buffer_offset = max_len;
	}

	/*
	 * Ensure that read/write operation that is calling this function
	 * doesn't exceed the end of its buffer.
	 */
	if ((int32_t)offset + (int32_t)len >= max_len)
		return EC_ERROR_OVERFLOW;
	/* Compute offset within the partition for the rd/wr operation */
	buffer_offset = start_offset + (int32_t)offset +
		sizeof(struct nvmem_tag);
	/* Make partition offset available to calling function */
	*p_buf_offset = buffer_offset;

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

	/* Get buffer length table info */
	nvmem_get_buffer_array(&p_buffer_tab, &nvmem_num_buffers);
	/* Default state for cache_base_ptr */
	cache_base_ptr = NULL;
	ret = nvmem_find_partition();
	if (ret != EC_SUCCESS) {
		CPRINTF("NvMem is corrupted\n");
		return ret;
	}

	return EC_SUCCESS;
}

int nvmem_read(unsigned int offset, unsigned int size,
		    void *data, enum nvmem_users user)
{
	int ret;
	uint8_t *p_src;
	uintptr_t src_addr;
	int32_t src_offset;

	/* Point to either NvMem flash or ram if that's active */
	if (cache_base_ptr == NULL)
		src_addr = NVMEM_BASE_ADDR + nvmem_act_partition *
			NVMEM_PARTITION_SIZE;

	else
		src_addr = (uintptr_t)cache_base_ptr;
	/* Get partition offset for this read operation */
	ret = nvmem_get_partition_off(user, offset, size, &src_offset);
	if (ret != EC_SUCCESS)
		return ret;
	/* Advance to the correct byte within the data buffer */
	src_addr += src_offset;
	p_src = (uint8_t *)src_addr;
	/* Copy from src into the caller's destination buffer */
	memcpy(data, p_src, size);

	return EC_SUCCESS;
}

int nvmem_write(unsigned int offset, unsigned int size,
		 void *data, enum nvmem_users user)
{
	int ret;
	uint8_t *p_dest;
	uintptr_t dest_addr;
	int32_t dest_offset;

	/* Make sure that the cache buffer is active */
	ret = nvmem_update_cache_ptr();
	if (ret)
		/* TODO: What to do when can't access cache buffer? */
		return ret;
	/* Compute partition offset for this write operation */
	ret = nvmem_get_partition_off(user, offset, size, &dest_offset);
	if (ret != EC_SUCCESS)
		return ret;
	/* Advance to correct offset within data buffer */
	dest_addr = (uintptr_t)cache_base_ptr;
	dest_addr += dest_offset;
	p_dest = (uint8_t *)dest_addr;
	/* Copy data from caller into destination buffer */
	memcpy(p_dest, data, size);

	return EC_SUCCESS;
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
