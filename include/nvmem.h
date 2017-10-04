/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * NVMEM: Non-volatile data system
 *
 * CrOS EC currently has access to a few non-volatile memories but they're not
 * consistently available:
 *
 * BBRAM: Requires power. Data is lost when power is removed by battery cutoff,
 * removal, or by hibernation on z-state system.
 *
 * EEPROM: Write-protect pin affects the entire region. Maskrom change is needed
 * to use half-region write-protect.
 *
 * NVMEM is a data system stored on a flash memory. It should behave more or
 * less the same on all platforms.
 *
 * Note that the write latency varies. So, it doesn't suit for application
 * requiring precise timing.
 */

#ifndef __CROS_EC_NVMEM_H
#define __CROS_EC_NVMEM_H

#include "common.h"
#include "datablob.h"

/**
 * Get data from NVMEM
 *
 * @param tag   Tag of the data being retrieved.
 * @param buf   Buffer where data is to be stored.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*.
 */
int nvmem_get(int tag, uint8_t *buf, uint8_t *size);

/**
 * Set data in NVMEM
 *
 * @param tag   Tag of the data being set.
 * @param buf   Data to be set in NVMEM.
 * @param size  Size of <buf> in bytes.
 * @return      EC_SUCCESS or EC_ERROR_*.
 */
int nvmem_set(int tag, const uint8_t *buf, uint8_t size);

#ifdef TEST_BUILD
/**
 * Test only declarations. Firmware shouldn't need them.
 */
extern const int nvmem_block_size;
extern const int nvmem_flash_offset;
int nvmem_create(void);
int nvmem_write(void);
int nvmem_get_offset(void);
int nvmem_get_cache_status(void);
void nvmem_reset(void);
#endif

#endif
