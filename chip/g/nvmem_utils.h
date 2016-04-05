/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_NVMEM_UTILS_H
#define __CROS_EC_NVMEM_UTILS_H

enum nvmem_users {
	NV_TPM = 0,
	NV_CR50,
	NV_NUM_USERS
};

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
 * @param user: Data section within NvMem space
 */
void nvmem_read(unsigned int startOffset, unsigned int size,
		void *data, enum nvmem_users user);

/**
 * Write 'size' amount of bytes to NvMem
 *
 * @param startOffset: Offset (in bytes) into NVmem logical space
 * @param size: Number of bytes to write
 * @param data: Pointer to source buffer
 * @param user: Data section within NvMem space
 */
void nvmem_write(unsigned int startOffset, unsigned int size,
		 void *data, enum nvmem_users user);


/**
 * Commit all previous NvMem writes to flash
 */
int nvmem_commit(void);

int nvmem_setup(void);

#endif /* __CROS_EC_NVMEM_UTILS_H */
