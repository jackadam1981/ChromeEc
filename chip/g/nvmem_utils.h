/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_NVMEM_UTILS_H
#define __CROS_EC_NVMEM_UTILS_H

/**
 * Initialize NVMem translation table and state variables
 *
 * @return EC_SUCCESS if a valid translation table is constructed, else
 *         error code.
 */
int nvmem_init(void);

/**
 * Read 'size' amount of bytes from NvMem
 *
 * @param startOffset: Offset (in bytes) into NVmem logical space
 * @param size: Number of bytes to read
 * @param data: Pointer to destination buffer
 */
void nvmem_read(unsigned int startOffset, unsigned int size, void *data);

/**
 * Write 'size' amount of bytes to NvMem
 *
 * @param startOffset: Offset (in bytes) into NVmem logical space
 * @param size: Number of bytes to write
 * @param data: Pointer to source buffer
 */
void nvmem_write(unsigned int startOffset, unsigned int size, void *data);

/**
 * Commit all previous NvMem writes to flash
 */
int nvmem_commit(void);

#endif /* __CROS_EC_NVMEM_UTILS_H */
