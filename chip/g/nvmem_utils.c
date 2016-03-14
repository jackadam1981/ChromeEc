/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Non-Volatile memory (NvMem) space is 16kB which is then divided into 8 -
 * 2kb blocks. The 2kB size is chosen to match Cr-50s minimum flash page erase
 * size. Within Cr-50 there are two customers for NvMem, 1) TPM2.0
 * specification and 2) Cr-50 specific paramters such as BIOS password storage.
 *
 * In order to maximize the amount of available NvMem, and provide robustness in
 * the event of critical failures such as loss of power during a flash
 * erase/write operation, NvMem space is broken up into 7 useable blocks and 1
 * spare block. The implementation uses the concept of logical Nv memory and
 * physical Nv memory. Physical blocks contain 2kB - sizeof(tag) bytes as shown
 * here.
 *
 *     Physical Block
 *     ---------------------------------------------------------------------
 *     |                2040 data bytes                      | 8 byte tag  |
 *     ---------------------------------------------------------------------
 *
 *     Physical Block Tag details
 *     ---------------------------------------------------------------------
 *     | index   | version | reserved      |           sha                 |
 *     ---------------------------------------------------------------------
 *         index     -> 1 byte logical block index
 *         version   -> 1 byte version number (0 - 0xfe)
 *         reserved  -> 2 bytes
 *         sha       -> 4 bytes of sha1 digest (1st 4 bytes in lieu of crc)
 *
 * Logical memory space consists of N physical blocks so that the amount of
 * memory avaialble to a user is N * 2040 bytes. These bytes are contiguous in a
 * logical sense. But, in actual NvMem, the logical memory consists of N
 * physical blocks which are out of order. There are two conversions/mappings
 * required to convert from a logical NvMem address to its physical memory
 * address. First, the logical memory address (offset into continguous NvMem
 * space) is converted into a logical block number and offset within that block
 * using the following computations:
 *
 *     Logical block number = (logical memory offset) / logical block size
 *     Logical block offset = (logical memory offset) % logical block size
 *
 * Next, the logical block number is mapped to its physical block number via a
 * translation table that is indexed via logical block numbers. The translation
 * table is constructed during the initialization function by reading each
 * physical block tag to determine which logical block number it represents.
 *
 * NvMem writes utilize a read/modify/write method. The translation table entry
 * for each logical block maintains a cache block index for the equivalent block
 * location within shared memory scratch ram. Once a particular
 * logical block is accessed for a write operation, the block will remain in
 * its cache block location until the commit() function is called. Read
 * operations can either read directly from flash memory, if the corresponding
 * logical block is not 'active', or will read from the cache ram location.
 *
 *     Logical | Physical | Cache   | Version
 *     Block   | Block    | Block   | Number
 *     Index   | Index    | Index   |
 *     --------------------------------------
 *        0    |          |         |
 *     --------------------------------------
 *        1    |          |         |
 *     --------------------------------------
 *        .         .          .         .
 *        .         .          .         .
 *        .         .          .         .
 *     --------------------------------------
 *        6    |          |         |
 *     --------------------------------------
 *
 * One spare physical block is maintained. When a commit() operation commences,
 * any cached logical block will be written into the spare physical block
 * location using a flash erase/write operation. The physical block which
 * correspnded to the logical block in cache then becomes the new spare physical
 * block location. Each time a logical block is committed to NvMem, its version
 * number is incremented. The version number can then be used during the next
 * initialization process to chose the newest logical block when two physical
 * block tags contain the same logical block number.
 */
#include <string.h>

#include "assert.h"
#include "common.h"
#include "console.h"
#include "dcrypto/dcrypto.h"
#include "flash.h"
#include "flash_config.h"
#include "timer.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

/* Struct for NV block tag */
struct nvmem_tag {
	int8_t block_index;
	uint8_t ver;
	uint16_t reserved;
	uint8_t sha[4];
};

/* NV Memory Block definitions */
#define NVMEM_START_ADDR (CONFIG_NV_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE)
#define NVMEM_PHY_BLOCK_SIZE CONFIG_FLASH_ERASE_SIZE
#define NVMEM_LOG_BLOCK_SIZE (NVMEM_PHY_BLOCK_SIZE - sizeof(struct nvmem_tag))
#define NVMEM_PHY_BLOCKS (CONFIG_NV_MEM_SIZE / NVMEM_PHY_BLOCK_SIZE)
#define NVMEM_CR50_BLOCKS 1
#define NVMEM_SPARE_BLOCKS 1
#define NVMEM_LOG_BLOCKS (NVMEM_PHY_BLOCKS - NVMEM_SPARE_BLOCKS)
#define NVMEM_TPM_BLOCKS (NVMEM_LOG_BLOCKS - NVMEM_CR50_BLOCKS)
#define NVMEM_NOT_INITIALIZED (-1)

/*
 * Macros to convert from logical offset to logical block index and logical
 * block offset. The logical block index is used to access the table that
 * maps the logical block number to physical block number.
 */
#define NVMEM_LOG_TO_BLK_IDX(offset) (offset / NVMEM_LOG_BLOCK_SIZE)
#define NVMEM_LOG_TO_BLK_OFF(offset) (offset % NVMEM_LOG_BLOCK_SIZE)

/* Structure for physical NvMem block */
struct nvmem_phy_block {
	uint8_t data[NVMEM_LOG_BLOCK_SIZE];
	struct nvmem_tag tag;
};

/* Structure for logical to physical block mappting table */
struct nvmem_block_map {
	int phy_block;
	int cache_block;
	uint8_t ver;
};

/* Table used to map logical blocks to physical blocks */
static struct nvmem_block_map log_to_phy_map[NVMEM_LOG_BLOCKS];
/* NVmem physical block that is avaialble */
static int nvmem_spare_block;

/* Scratch buffer definitions */
/* TODO - This will be removed when the shared memory buffers are used */
#define CACHE_RAM_MAX_INDEX 4
static uint8_t cache_ram[NVMEM_PHY_BLOCK_SIZE * CACHE_RAM_MAX_INDEX];
static int cache_ram_index;

/* TODO - This function is not required, but useful for initial debugging */
static void fill_block(uint8_t pattern, int size)
{
	int n;

	if (size > NVMEM_LOG_BLOCK_SIZE) {
		size = NVMEM_LOG_BLOCK_SIZE;
		CPRINTF("Block size truncated to 0x%x\n", size);
	}
	for (n = 0; n < size; n++)
		cache_ram[n] = pattern;
}

static int verify_log_to_phy_map(void)
{
	int block;
	int phy_blocks[NVMEM_PHY_BLOCKS];
	int phy_idx;

	for (block = 0; block < NVMEM_PHY_BLOCKS; block++) {
		/* init verification table */
		phy_blocks[block] = NVMEM_NOT_INITIALIZED;
	}

	/* Validate table mapping. */
	for (block = 0; block < NVMEM_LOG_BLOCKS; block++) {
		phy_idx = log_to_phy_map[block].phy_block;
		/*
		 * Each logical block should be mapped to one and only one phy
		 * block number. phy_idx is the phy block number for the logical
		 * block indexed by 'block'. Therefore, this entry in phy_blocks
		 * should have the value NVMEM_NOT_INITIALIZED. If it is not
		 * that, then this phy block number has already been encountered
		 * and at least two logical blocks are pointing to the same phy
		 * block location which is an invalid state.
		 */
		if (phy_blocks[phy_idx] != NVMEM_NOT_INITIALIZED) {
			CPRINTF("NvMem: Phy Block Error!! mapping is wrong\n");
			return EC_ERROR_UNKNOWN;
		}

		phy_blocks[phy_idx] = block;
	}

	/*
	 * After retrieving the phy block number for each logical block,
	 * there should only be one row of phy_blocks[] that still is still
	 * equal to NVMEM_NOT_INITIALIZED and this entry should be indexed by
	 * the global variable nvmem_spare_block which will be the 1st phy block
	 * used when an updated block needs to be written to flash.
	 */
	if (phy_blocks[nvmem_spare_block] != NVMEM_NOT_INITIALIZED) {
		CPRINTF("NvMem: Log to Phy Map is inconsistent\n");
		return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

static void nvmem_compute_sha(uint8_t *p_buf, int num_bytes, uint8_t *p_sha)
{
	uint8_t sha1_digest[SHA1_DIGEST_SIZE];
	/*
	 * Taking advantage of the built in dcrypto engine to generate
	 * a CRC-like value that can be used to validate contents of each
	 * NvMem block. Only using the lower 4 bytes of the sha1 hash.
	 */
	DCRYPTO_SHA1_hash((uint8_t *)p_buf,
			  num_bytes,
			  sha1_digest);
	memcpy(p_sha, sha1_digest, 4);
}

static int nvmem_verify_block_sha(struct nvmem_phy_block *p_block)
{
	uint8_t sha_comp[4];

	/* Number of bytes to compute sha over */
	nvmem_compute_sha((uint8_t *)p_block,
				     (NVMEM_PHY_BLOCK_SIZE - sizeof(sha_comp)),
				     sha_comp);
	/* Check if computed value matches stored value. */
	return memcmp(p_block->tag.sha, sha_comp, 4);
}

static int nvmem_setup(void)
{
	int block;
	struct nvmem_tag *p_tag;
	struct nvmem_phy_block *p_block;
	int nvmem_offset;

	CPRINTF("Configuring NVMEM FLash Blocks\n");
	/*
	 * Initialize NVmem blocks. This function will only be called
	 * if during nvmem_init() a valid translation table can't be
	 * built. A one to one mapping is assumed, and all block version
	 * numbers are reset to 0.
	 */
	for (block = 0; block < NVMEM_LOG_BLOCKS; block++) {
		/* TODO for debug only fill blocks with a known pattern */
		fill_block(block << 4 | block, NVMEM_LOG_BLOCK_SIZE);

		/* Create pointer to beginning of block tag */
		p_block = (struct nvmem_phy_block *)cache_ram;
		p_tag = &p_block->tag;
		/* Populate the block tag */
		p_tag->ver = 0;
		p_tag->block_index = block;
		p_tag->reserved = 0;
		nvmem_compute_sha(cache_ram,
				  NVMEM_PHY_BLOCK_SIZE - sizeof(p_tag->sha),
				  p_tag->sha);
		/* Nvmem block is now ready, write it to flash. */
		nvmem_offset = CONFIG_NV_MEM_OFF + block * NVMEM_PHY_BLOCK_SIZE;
		/* Erase block. */
		if (flash_physical_erase(nvmem_offset,
					 NVMEM_PHY_BLOCK_SIZE)) {
			CPRINTF("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_UNKNOWN;
		}
		/* Write the new contents of the block. */
		if (flash_physical_write(nvmem_offset,
				     NVMEM_PHY_BLOCK_SIZE,
					 (const char *)cache_ram)) {
			CPRINTF("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_UNKNOWN;
		}
	}
	return EC_SUCCESS;
}

static void nvmem_resolve_dup(int log_block, uint8_t tab_ver,
			      uint8_t block_ver, int phy_block)
{
	/*
	 * Two physical blocks have the same logical block index. Use
	 * their version numbers to determine which is newest. Version
	 * numbers update circularly.
	 */
	uint8_t ver_delta;

	ver_delta = (block_ver - tab_ver + 256) & 0xff;
	if (ver_delta < 0x80) {
		/* The physical block has the newest version */
		/* Get the current phy block table entry */
		nvmem_spare_block = log_to_phy_map[log_block].phy_block;
		/* Update the table entries */
		log_to_phy_map[log_block].phy_block = phy_block;
		log_to_phy_map[log_block].ver = block_ver;
	} else
		/* The table entry is newest, so spare is phy_block */
		nvmem_spare_block = phy_block;
}

static int nvmem_validate_phy_blocks(void)
{
	int log_index;
	int block;
	int ret;
	int num_bad_blocks;
	struct nvmem_phy_block *p_block;
	struct nvmem_tag *p_tag;

	p_block = (struct nvmem_phy_block *)(NVMEM_START_ADDR);
	num_bad_blocks = 0;
	/* Scan all physical NvMem blocks and build translation table */
	for (block = 0; block < NVMEM_PHY_BLOCKS; block++) {
		p_tag = &p_block->tag;
		log_index = p_tag->block_index;
		/* Check if block sha is valid */
		ret = nvmem_verify_block_sha(p_block);
		if (ret == EC_SUCCESS) {
			/* See if entry for this logical block already exists */
			if (log_to_phy_map[log_index].phy_block !=
			    NVMEM_NOT_INITIALIZED)
				/* Entry for this logical block exists. */
				nvmem_resolve_dup(log_index,
						  log_to_phy_map[log_index].ver,
						  p_tag->ver,
						  block);
			else {
				/* Table entry is empty, update this slot. */
				log_to_phy_map[log_index].phy_block = block;
				log_to_phy_map[log_index].ver = p_tag->ver;
			}
		} else
			if (++num_bad_blocks > NVMEM_SPARE_BLOCKS)
				return EC_ERROR_CRC;
		p_block++;
	}
	/* All NVMem logical blocks are inactive. */
	cache_ram_index = 0;
	/* Handle case where no duplicates were found */
	if (nvmem_spare_block == NVMEM_NOT_INITIALIZED)
		nvmem_spare_block = NVMEM_LOG_BLOCKS;

	/*
	 * The translation table should be fully built at this point. Double
	 * check that each logical block has been assigned a physical block
	 * number.
	 */
	for (block = 0; block < NVMEM_LOG_BLOCKS; block++) {
		if (log_to_phy_map[block].phy_block == NVMEM_NOT_INITIALIZED)
			return EC_ERROR_UNKNOWN;
	}

	return EC_SUCCESS;
}

int nvmem_init(void)
{
	int block;

	/*
	 * Initialize the NVMem translation table and related variables
	 * which are required to maintain the logical to physical addressing
	 * scheme. If the NVMem tags are not setup, or if the blocks
	 * have invalid sha values, a setup routine is called which
	 * clears all of NVMem and configures the tags to default values.
	 */

	/* Don't know which is the spare block at this point. */
	nvmem_spare_block = NVMEM_NOT_INITIALIZED;
	/* Set translation table entries to not initialized */
	for (block = 0; block < (NVMEM_TPM_BLOCKS + 1); block++) {
		log_to_phy_map[block].phy_block = NVMEM_NOT_INITIALIZED;
		log_to_phy_map[block].cache_block = NVMEM_NOT_INITIALIZED;
	}
	/* Attempt to build the translation table. */
	if (nvmem_validate_phy_blocks()) {
		CPRINTF("NVMEM Blocks not initialized!\n");
		/* Configure NVMEM blocks. */
		if (nvmem_setup())
			return EC_ERROR_UNKNOWN;
		/* Attempt to validate one more time. */
		if (nvmem_validate_phy_blocks()) {
			CPRINTF("Failed to configure NVMEM blocks!\n");
			return EC_ERROR_UNKNOWN;
		}
	}
	/*
	 * If reached here, then NVMEM blocks have been validated and the
	 * translation table has been properly initialized.
	 */
	verify_log_to_phy_map();

	return EC_SUCCESS;
}

void nvmem_read(unsigned int startOffset, unsigned int size,
	       void *data)
{
	int block_offset;
	int block_index;
	int copy_len;
	int scratch_idx;
	uint8_t *p_src;
	/*
	 * Data may all be in physical flash, or some/all could be
	 * in a scratch buffer locations. If scratch buffer location
	 * is active, then read from that instead of flash memory.
	 */
	while (size) {
		/*
		 * Translate logical memory offset into a logical block
		 * number and logical block offset.
		 */
		block_index = NVMEM_LOG_TO_BLK_IDX(startOffset);
		block_offset = NVMEM_LOG_TO_BLK_OFF(startOffset);
		scratch_idx = log_to_phy_map[block_index].cache_block;
		if (scratch_idx >= 0)
			/* This block is in scratch buf memory. */
			p_src = &cache_ram[scratch_idx * NVMEM_PHY_BLOCK_SIZE +
					   block_offset];
		else
			/* This block can be read directly from flash. */
			p_src = (uint8_t *)(NVMEM_START_ADDR +
				 log_to_phy_map[block_index].phy_block *
				 NVMEM_PHY_BLOCK_SIZE + block_offset);
		/* Determine number of bytes that can be read from block */
		copy_len = MIN(size, NVMEM_LOG_BLOCK_SIZE - block_offset);
		/* Read data. */
		memcpy(data, p_src, copy_len);
		/* Account for number of bytes consumed */
		startOffset += copy_len;
		data += copy_len;
		size -= copy_len;
	}
}

void nvmem_write(unsigned int startOffset, unsigned int size, void *data)
{
	int block_offset;
	int block_index;
	int copy_len;
	int scratch_idx;
	uint8_t *p_src;
	uint8_t *p_dest;

	/*
	 * Writes always go in the scratch buffer. If a particular phy flash
	 * block is not active, then first it must be copied into the scratch
	 * buffer, prior to any write taking place.
	 *
	 * This function does not write back to flash memory. That step is
	 * taken when the 'commit()' function is called.
	 */

	while (size) {
		/*
		 * Translate logical memory offset into a logical block
		 * number and logical block offset.
		 */
		block_index = NVMEM_LOG_TO_BLK_IDX(startOffset);
		block_offset = NVMEM_LOG_TO_BLK_OFF(startOffset);
		scratch_idx = log_to_phy_map[block_index].cache_block;

		/* First check if the current phy flash block is active. */
		if (scratch_idx == NVMEM_NOT_INITIALIZED) {
			/* Need to copy from phy memory to scratch buffer */
			p_src = (uint8_t *)(NVMEM_START_ADDR +
				log_to_phy_map[block_index].phy_block *
				NVMEM_PHY_BLOCK_SIZE);
			assert(cache_ram_index < CACHE_RAM_MAX_INDEX);
			p_dest = cache_ram + cache_ram_index *
				NVMEM_PHY_BLOCK_SIZE;
			/* Copy entire physical flash block. */
			memcpy(p_dest, p_src, NVMEM_PHY_BLOCK_SIZE);
			/* Save current index and update */
			log_to_phy_map[block_index].cache_block =
				cache_ram_index++;
		}
		/* At this point, the current flash block is in scractch buf. */
		p_dest = cache_ram +
			log_to_phy_map[block_index].cache_block *
			NVMEM_PHY_BLOCK_SIZE + block_offset;
		p_src = data;
		/* Copy as much data as possible (until end of block) */
		copy_len = MIN(size, NVMEM_LOG_BLOCK_SIZE - block_offset);
		/* Write data to scratch block. */
		memcpy(p_dest, p_src, copy_len);
		/* Account for bytes that were written. */
		startOffset += copy_len;
		size -= copy_len;
		data += copy_len;
	}
}

int nvmem_commit(void)
{
	int block;
	int cache_block_idx;
	int cache_offset;
	int nvmem_block_addr;
	int nvmem_offset;
	int new_spare_block;
	struct nvmem_tag *p_tag;
	struct nvmem_phy_block *p_block;

	/*
	 * All scratch buffer blocks must be written to physical flash
	 * memory. In addition, the scratch block buffer index table
	 * entries must be reset along with the index itself.
	 */
	for (block = 0; block < NVMEM_TPM_BLOCKS; block++) {
		if (log_to_phy_map[block].cache_block >= 0) {
			cache_block_idx =
				log_to_phy_map[block].cache_block;
			cache_offset = NVMEM_PHY_BLOCK_SIZE *
				cache_block_idx;
			/*
			 * Compare the block in ram with the one that is
			 * in flash to check if any data has changed.
			 */
			nvmem_block_addr = NVMEM_START_ADDR +
				log_to_phy_map[block].phy_block *
				NVMEM_PHY_BLOCK_SIZE;
			if (!memcmp(&cache_ram[cache_offset],
				    (uint8_t *)nvmem_block_addr,
				    NVMEM_LOG_BLOCK_SIZE))
				continue;

			/*
			 * Only get here if the scratch block is different than
			 * the copy already in flash. Need to update version
			 * number and write scratch block to the spare flash
			 * block location and then update the spare block
			 * number.
			 */
			p_block = (struct nvmem_phy_block *)(cache_ram +
							     cache_offset);
			p_tag = &p_block->tag;
			/* Update the block version number */
			p_tag->ver++;
			/* 0xff is not a valid version number */
			if (p_tag->ver == 0xff)
				p_tag->ver++;
			/* Get updated sha value. */
			nvmem_compute_sha(cache_ram + cache_offset,
					  NVMEM_PHY_BLOCK_SIZE -
					  sizeof(p_tag->sha),
					  p_tag->sha);
			/* Nvmem block is now ready, write it to flash. */
			nvmem_offset = CONFIG_NV_MEM_OFF + nvmem_spare_block *
				NVMEM_PHY_BLOCK_SIZE;

			if (flash_physical_erase(nvmem_offset,
						 NVMEM_PHY_BLOCK_SIZE)) {
				CPRINTF("%s:%d\n", __func__, __LINE__);
				return EC_ERROR_UNKNOWN;
			}
			if (flash_physical_write(nvmem_offset,
						 NVMEM_PHY_BLOCK_SIZE,
						 &cache_ram[cache_offset])) {
				CPRINTF("%s:%d\n", __func__, __LINE__);
				return EC_ERROR_UNKNOWN;
			}
			/*
			 * New spare block is now the phy block that was being
			 * used by this logical block as indicated in the
			 * table entry for this logical block. Need to update
			 * variable being used to track the spare phy block
			 * prior to updating the translation table entries.
			 */
			new_spare_block = log_to_phy_map[block].phy_block;
			/* Update translation table entries */
			log_to_phy_map[block].cache_block =
				NVMEM_NOT_INITIALIZED;
			log_to_phy_map[block].phy_block = nvmem_spare_block;
			log_to_phy_map[block].ver = p_tag->ver;
			/* spare block is now the one that was just written */
			nvmem_spare_block = new_spare_block;
			/* TODO -> This is debug only, not required */
			verify_log_to_phy_map();
		}
	}
	/* Free up scratch buffers */
	cache_ram_index = 0;

	return EC_SUCCESS;
}
