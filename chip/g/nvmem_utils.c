/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 *  <Add description here>
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
	uint8_t block_index;
	uint8_t ver;
	uint16_t reserved;
	uint32_t sha;
};

/* Structure for translation table */
struct nvmem_translate {
	int16_t phy_block;
	int16_t scratch_block;
	uint8_t ver;
};

/* NV Memory Block definitions */
#define NVMEM_START_ADDR (CONFIG_NV_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE)
#define NVMEM_PHY_BLOCKSIZE CONFIG_FLASH_ERASE_SIZE
#define NVMEM_LOGICAL_SIZE (NVMEM_PHY_BLOCKSIZE - sizeof(struct nvmem_tag))
#define NVMEM_NUMBLOCKS (CONFIG_NV_MEM_SIZE / NVMEM_PHY_BLOCKSIZE)
#define NVMEM_CR50_NUMBLOCKS 1
#define NVMEM_SPARE_NUMBLOCKS 1
#define NVMEM_NUM_LOGICAL_BLOCKS (NVMEM_NUMBLOCKS - NVMEM_SPARE_NUMBLOCKS)
#define NVMEM_TPM_NUMBLOCKS (NVMEM_NUM_LOGICAL_BLOCKS - NVMEM_CR50_NUMBLOCKS)
#define NVMEM_NOT_INITIALIZED (-1)

/* Macros to convert from logical offset to block index and offset */
#define NVMEM_LOG_TO_BLK_IDX(offset) (offset / NVMEM_LOGICAL_SIZE)
#define NVMEM_LOG_TO_BLK_OFF(offset) (offset % NVMEM_LOGICAL_SIZE)

/* Translation table used for logical to physical NVmem computations */
static struct nvmem_translate nvmem_block_tab[NVMEM_NUM_LOGICAL_BLOCKS];
/* NVmem physical block that is avaialble */
static int8_t nvmem_spare_block;

/* Scratch buffer definitions */
/* TODO - This will be removed when the shared memory buffers are used */
#define BLOCK_RAM_MAX_INDEX 4
static uint8_t block_ram[NVMEM_PHY_BLOCKSIZE * BLOCK_RAM_MAX_INDEX];
static uint8_t block_ram_index;

/* TODO - This function is not required, but useful for initial debugging */
static void fill_block(uint8_t pattern, int size)
{
	int n;

	if (size > NVMEM_LOGICAL_SIZE) {
		size = NVMEM_LOGICAL_SIZE;
		CPRINTF("Block size truncated to 0x%x\n", size);
	}
	for (n = 0; n < size; n++)
		block_ram[n] = pattern;
}

static void verify_translation_table(void)
{
	int block;
	int phy_blocks[NVMEM_NUMBLOCKS];
	int phy_idx;

	for (block = 0; block < NVMEM_NUMBLOCKS; block++) {
		/* init verification table */
		phy_blocks[block] = NVMEM_NOT_INITIALIZED;
	}

	/* Validate table mapping and print entries. */
	for (block = 0; block < NVMEM_NUM_LOGICAL_BLOCKS; block++) {
		phy_idx = nvmem_block_tab[block].phy_block;
		if (phy_blocks[phy_idx] != NVMEM_NOT_INITIALIZED)
			CPRINTF("Phy Block Error!! mapping is wrong\n");
		assert(phy_blocks[phy_idx] == NVMEM_NOT_INITIALIZED);
		phy_blocks[phy_idx] = block;
	}

	/* The one phy block not used should = nvmem_spare_block */
	assert(phy_blocks[nvmem_spare_block] == NVMEM_NOT_INITIALIZED);
}

static uint32_t nvmem_compute_sha(uint8_t *p_buf, int num_bytes)
{
	uint8_t sha1_digest[SHA1_DIGEST_SIZE];
	/*
	 * Taking advantage of the built in decrypto engine to generate
	 * a CRC-like value that can be used to validate contents of each
	 * NvMem block. Only using the lower 4 bytes of the sha1 hash.
	 */
	DCRYPTO_SHA1_hash((uint8_t *)p_buf,
			  num_bytes,
			  sha1_digest);
	return *(uint32_t *)sha1_digest;
}

static int nvmem_verify_block_sha(int block_addr)
{
	struct nvmem_tag *p_tag;
	uint32_t sha_comp;

	/* Point to tag for this block in flash memory */
	p_tag = (struct nvmem_tag *)(block_addr + NVMEM_PHY_BLOCKSIZE -
				     sizeof(struct nvmem_tag));
	/* Compute sha1 of data contianed in block */
	sha_comp = nvmem_compute_sha((uint8_t *)block_addr,
				     (NVMEM_PHY_BLOCKSIZE - sizeof(sha_comp)));
	/* Check if computed value matches stored value. */
	return sha_comp == p_tag->sha ? EC_SUCCESS : EC_ERROR_CRC;
}

int nvmem_setup(void)
{
	int block;
	struct nvmem_tag *p_tag;
	int nvmem_offset;

	CPRINTF("Configuring NVMEM FLash Blocks\n");
	/*
	 * Initialize NVmem blocks. This function will only be called
	 * if during nvmem_init() a valid translation table can't be
	 * built. A one to one mapping is assumed, and all block version
	 * numbers are reset to 0.
	 */
	for (block = 0; block < NVMEM_NUM_LOGICAL_BLOCKS; block++) {
		/* TODO for debug only fill blocks with a known pattern */
		fill_block(block<<4|(block), NVMEM_LOGICAL_SIZE);

		/* Create pointer to beginning of block tag */
		p_tag = (struct nvmem_tag *)(block_ram + NVMEM_PHY_BLOCKSIZE -
					     sizeof(struct nvmem_tag));
		/* Populate the block tag */
		p_tag->ver = 0;
		p_tag->block_index = block;
		p_tag->reserved = 0;
		p_tag->sha = nvmem_compute_sha(block_ram,
						NVMEM_PHY_BLOCKSIZE -
					       sizeof(p_tag->sha));
		/* Nvmem block is now ready, write it to flash. */
		nvmem_offset = CONFIG_NV_MEM_OFF + block * NVMEM_PHY_BLOCKSIZE;
		/* Erase block. */
		if (flash_physical_erase(nvmem_offset,
					 NVMEM_PHY_BLOCKSIZE)) {
			CPRINTF("%s:%d\n", __func__, __LINE__);
			return EC_ERROR_UNKNOWN;
		}
		/* Write the new contents of the block. */
		if (flash_physical_write(nvmem_offset,
				     NVMEM_PHY_BLOCKSIZE,
					 (const char *)block_ram)) {
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
		nvmem_spare_block = nvmem_block_tab[log_block].phy_block;
		/* Update the table entries */
		nvmem_block_tab[log_block].phy_block = phy_block;
		nvmem_block_tab[log_block].ver = block_ver;
	} else
		/* The table entry is newest, so spare is phy_block */
		nvmem_spare_block = phy_block;
}

static int nvmem_validate_phy_blocks(void)
{
	int8_t log_index;
	int block;
	int block_phy_addr;
	int ret;
	int num_bad_blocks;
	struct nvmem_tag *p_tag;

	block_phy_addr = CONFIG_NV_MEM_OFF + CONFIG_PROGRAM_MEMORY_BASE;
	num_bad_blocks = 0;
	/* Scan all physical NvMem blocks and build translation table */
	for (block = 0; block < NVMEM_NUMBLOCKS; block++) {
		p_tag = (struct nvmem_tag *)(block_phy_addr +
					     NVMEM_PHY_BLOCKSIZE -
					     sizeof(struct nvmem_tag));
		log_index = p_tag->block_index;
		/* Check if block sha is valid */
		ret = nvmem_verify_block_sha(block_phy_addr);
		if (ret == EC_SUCCESS) {
			/* See if entry for this logical block already exists */
			if (nvmem_block_tab[log_index].phy_block !=
			    NVMEM_NOT_INITIALIZED)
				/* Entry for this logical block exists. */
				nvmem_resolve_dup(log_index,
						  nvmem_block_tab[log_index].ver,
						  p_tag->ver,
						  block);
			else {
				/* Table entry is empty, update this slot. */
				nvmem_block_tab[log_index].phy_block = block;
				nvmem_block_tab[log_index].ver = p_tag->ver;
			}
		} else
			if (++num_bad_blocks > NVMEM_SPARE_NUMBLOCKS)
				return EC_ERROR_CRC;
		block_phy_addr += NVMEM_PHY_BLOCKSIZE;
	}
	/* All NVMem logical blocks are inactive. */
	block_ram_index = 0;
	/* Handle case where no duplicates were found */
	if (nvmem_spare_block == NVMEM_NOT_INITIALIZED)
		nvmem_spare_block = NVMEM_NUM_LOGICAL_BLOCKS;

	/*
	 * The translation table should be fully built at this point. Double
	 * check that each logical block has been assigned a physical block
	 * number.
	 */
	for (block = 0; block < NVMEM_NUM_LOGICAL_BLOCKS; block++) {
		if (nvmem_block_tab[block].phy_block == NVMEM_NOT_INITIALIZED)
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
	for (block = 0; block < (NVMEM_TPM_NUMBLOCKS + 1); block++) {
		nvmem_block_tab[block].phy_block = NVMEM_NOT_INITIALIZED;
		nvmem_block_tab[block].scratch_block = NVMEM_NOT_INITIALIZED;
	}
	/* Attempt to build the translation table. */
	if (nvmem_validate_phy_blocks()) {
		CPRINTF("NVMEM Blocks not initialized!\n");
		/* Configure NVMEM blocks. */
		nvmem_setup();
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
	verify_translation_table();

	return EC_SUCCESS;
}

void nvmem_read(unsigned int startOffset, unsigned int size,
	       void *data)
{
	uint32_t block_offset;
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
		scratch_idx = nvmem_block_tab[block_index].scratch_block;
		if (scratch_idx >= 0)
			/* This block is in scratch buf memory. */
			p_src = &block_ram[scratch_idx * NVMEM_PHY_BLOCKSIZE +
					   block_offset];
		else
			/* This block can be read directly from flash. */
			p_src = (uint8_t *)(NVMEM_START_ADDR +
				 nvmem_block_tab[block_index].phy_block *
				 NVMEM_PHY_BLOCKSIZE + block_offset);
		/* Determine number of bytes that can be read from block */
		copy_len = MIN(size, NVMEM_LOGICAL_SIZE - block_offset);
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
	uint32_t block_offset;
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
		scratch_idx = nvmem_block_tab[block_index].scratch_block;

		/* First check if the current phy flash block is active. */
		if (scratch_idx == NVMEM_NOT_INITIALIZED) {
			/* Need to copy from phy memory to scratch buffer */
			p_src = (uint8_t *)(NVMEM_START_ADDR +
				nvmem_block_tab[block_index].phy_block *
				NVMEM_PHY_BLOCKSIZE);
			assert(block_ram_index < BLOCK_RAM_MAX_INDEX);
			p_dest = block_ram + block_ram_index *
				NVMEM_PHY_BLOCKSIZE;
			/* Copy entire physical flash block. */
			memcpy(p_dest, p_src, NVMEM_PHY_BLOCKSIZE);
			/* Save current index and update */
			nvmem_block_tab[block_index].scratch_block =
				block_ram_index++;
		}
		/* At this point, the current flash block is in scractch buf. */
		p_dest = block_ram +
			nvmem_block_tab[block_index].scratch_block *
			NVMEM_PHY_BLOCKSIZE + block_offset;
		p_src = data;
		/* Copy as much data as possible (until end of block) */
		copy_len = MIN(size, NVMEM_LOGICAL_SIZE - block_offset);
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
	int scratch_block_idx;
	uint32_t scratch_offset;
	struct nvmem_tag *p_tag;
	int nvmem_offset;
	int new_spare_block;

	/*
	 * All scratch buffer blocks must be written to physical flash
	 * memory. In addition, the scratch block buffer index table
	 * entries must be reset along with the index itself.
	 */
	for (block = 0; block < NVMEM_TPM_NUMBLOCKS; block++) {
		if (nvmem_block_tab[block].scratch_block >= 0) {
			scratch_block_idx =
				nvmem_block_tab[block].scratch_block;
			scratch_offset = NVMEM_PHY_BLOCKSIZE *
				scratch_block_idx;
			/*
			 * Compare the block in ram with the one that is
			 * in flash to check if any data has changed.
			 */
			nvmem_offset = NVMEM_START_ADDR +
				nvmem_block_tab[block].phy_block *
				NVMEM_PHY_BLOCKSIZE;
			if (!memcmp(&block_ram[scratch_offset],
				    (uint8_t *)nvmem_offset,
				    NVMEM_LOGICAL_SIZE))
				continue;

			/*
			 * Only get here if the scratch block is different than
			 * the copy already in flash. Need to update version
			 * number and write scratch block to the spare flash
			 * block location and then update the spare block
			 * number.
			 */
			p_tag = (struct nvmem_tag *)(block_ram +
						     scratch_offset +
						     NVMEM_PHY_BLOCKSIZE -
						     sizeof(struct nvmem_tag));
			/* Update the block version number */
			p_tag->ver++;
			/* 0xff is not a valid version number */
			if (p_tag->ver == 0xff)
				p_tag->ver++;
			/* Get updated sha value. */
			p_tag->sha = nvmem_compute_sha((block_ram +
							scratch_offset),
							NVMEM_PHY_BLOCKSIZE -
						       sizeof(p_tag->sha));
			/* Nvmem block is now ready, write it to flash. */
			nvmem_offset = CONFIG_NV_MEM_OFF + nvmem_spare_block *
				NVMEM_PHY_BLOCKSIZE;

			if (flash_physical_erase(nvmem_offset,
						 NVMEM_PHY_BLOCKSIZE)) {
				CPRINTF("%s:%d\n", __func__, __LINE__);
				return EC_ERROR_UNKNOWN;
			}
			if (flash_physical_write(nvmem_offset,
						 NVMEM_PHY_BLOCKSIZE,
						 &block_ram[scratch_offset])) {
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
			new_spare_block = nvmem_block_tab[block].phy_block;
			/* Update translation table entries */
			nvmem_block_tab[block].scratch_block =
				NVMEM_NOT_INITIALIZED;
			nvmem_block_tab[block].phy_block = nvmem_spare_block;
			nvmem_block_tab[block].ver = p_tag->ver;
			/* spare block is now the one that was just written */
			nvmem_spare_block = new_spare_block;
			/* TODO -> This is debug only, not required */
			verify_translation_table();
		}
	}
	/* Free up scratch buffers */
	block_ram_index = 0;

	return EC_SUCCESS;
}
