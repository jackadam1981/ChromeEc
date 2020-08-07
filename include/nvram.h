/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NVRAM: Non-volatile RAM
 *
 * CrOS EC currently has access to a few non-volatile memories but their
 * availabilities are limited:
 *
 * BBRAM: Requires power. Data is lost when power is removed by battery cutoff,
 * removal, or by hibernation on z-state system.
 *
 * EEPROM: Write-protect pin affects the entire region. Maskrom change is needed
 * to use half-region write-protect.
 *
 * NVRAM is a data system stored on a flash memory. NVRAM is persistently and
 * consistently available on all platforms.
 *
 * Note that the write latency varies. So, it doesn't suit for application
 * requiring precise timing.
 */

#ifndef __CROS_EC_NVRAM_H
#define __CROS_EC_NVRAM_H

#include "common.h"
#include "datablob.h"

#ifndef NVRAM_BLOCK_SIZE
#define NVRAM_BLOCK_SIZE	0x1000
#endif

#ifndef NVRAM_FLASH_OFF
#define NVRAM_FLASH_OFF		(CONFIG_EC_WRITABLE_STORAGE_OFF \
				 + CONFIG_EC_WRITABLE_STORAGE_SIZE \
				 - NVRAM_BLOCK_SIZE)
#endif

#ifndef NVRAM_RECORD_SIZE
#define NVRAM_RECORD_SIZE	256
#endif

/*
 * Datablob Tags
 */
enum nvram_tag {
	NVRAM_TAG_BATTERY_CAPACITY = 0,
	NVRAM_TAG_SHUTDOWN_DURATION = 1,
	NVRAM_TAG_COUNT = 255,
	/* No more after this */
};
BUILD_ASSERT(NVRAM_TAG_COUNT <= UINT8_MAX);

/**
 * Get data from NVRAM
 *
 * @param tag   Tag of the data being retrieved.
 * @param buf   Buffer where data is to be stored.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*.
 */
int nvram_get(int tag, uint8_t *buf, uint8_t *size);

/**
 * Set data in NVRAM
 *
 * @param tag   Tag of the data being set.
 * @param buf   Data to be set in NVRAM.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*.
 */
int nvram_set(int tag, const uint8_t *buf, uint8_t size);

#ifdef TEST_BUILD
/**
 * Test only declarations. Firmware shouldn't need them.
 */
extern const int nvram_block_size;
extern const int nvram_flash_offset;
int nvram_create(void);
int nvram_write(void);
int nvram_get_offset(void);
int nvram_get_cache_status(void);
void nvram_reset(void);
#endif

#endif
