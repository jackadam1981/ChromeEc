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
#define NVMEM_NUM_PARTITIONS 2
#define NVMEM_PARTITION_SIZE (CONFIG_NV_MEM_SIZE / NVMEM_NUM_PARTITIONS)

#define NVMEM_ACQUIRE_CACHE_SLEEP_MS 20
#define NVMEM_ACQUIRE_CACHE_MAX_ATTEMPTS (200 / NVMEM_ACQUIRE_CACHE_SLEEP_MS)


/* Structure for physical NvMem block */
struct nvmem_phy_block {
	struct nvmem_tag tag;
	uint8_t data[NVMEM_PARTITION_SIZE - sizeof(struct nvmem_tag)];
};


/* NvMem Cache Memory pointer */
static uint8_t *cache_base_ptr;

/* Debug only, control CPRINTF print level */
static int nvmem_dump;


static int nvmem_acquire_cache(void)
{
	int attempts = 0;
	int avail_cache_size;
	int ret;

	avail_cache_size = shared_mem_size();
	if (avail_cache_size < NVMEM_PARTITION_SIZE) {
		CPRINTF("Not enough cache! avail = 0x%x, part = 0x%x\n",
			avail_cache_size, NVMEM_PARTITION_SIZE);
		return EC_ERROR_OVERFLOW;
	}

	while (attempts < NVMEM_ACQUIRE_CACHE_MAX_ATTEMPTS) {
		ret = shared_mem_acquire(NVMEM_PARTITION_SIZE,
					 (char **)&cache_base_ptr);
		if (ret == EC_SUCCESS)
			return EC_SUCCESS;
		else if (ret == EC_ERROR_BUSY)
			/* wait NVMEM_ACQUIRE_CACHE_SLEEP_MS  msec */
			/* TODO what time really makes sense?? */
			msleep(NVMEM_ACQUIRE_CACHE_SLEEP_MS);
		attempts++;
	}
	/* Cache ram was not available, can't do writes */
	/* TODO what to do in this error case? */
	return EC_ERROR_TIMEOUT;
}

static int nvmem_release_cache(void)
{
	shared_mem_release(cache_base_ptr);
	cache_base_ptr = NULL;
	if (nvmem_dump >= 1)
		CPRINTF("release_cache()\n");
	return EC_SUCCESS;
}

int nvmem_setup(void)
{
	int ret;

	ret = nvmem_acquire_cache();
	if (ret != EC_SUCCESS) {
		CPRINTF("NvMem: Cache ram not available!\n");
		return ret;
	}

	return EC_SUCCESS;
}

int nvmem_init(void)
{
	cache_base_ptr = NULL;
	return EC_SUCCESS;
}

void nvmem_read(unsigned int startOffset, unsigned int size,
	       void *data)
{

}

void nvmem_write(unsigned int startOffset, unsigned int size, void *data)
{

}

int nvmem_commit(void)
{

	/* Free up scratch buffers */
	nvmem_release_cache();

	return EC_SUCCESS;
}
