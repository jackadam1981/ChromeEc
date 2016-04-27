/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_NVMEM_UTILS_H
#define __CROS_EC_NVMEM_UTILS_H

/* Struct for NV block tag */
struct nvmem_tag {
	uint8_t sha[NVMEM_SHA_SIZE];
	uint16_t version;
	uint16_t reserved;
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
 * @return EC_ERROR_OVERFLOW (non-zero) if the read operation would exceed the
 *         buffer length of the given user, otherwise EC_SUCCESS.
 */
int nvmem_read(unsigned int startOffset, unsigned int size,
		void *data, enum nvmem_users user);

/**
 * Write 'size' amount of bytes to NvMem
 *
 * @param startOffset: Offset (in bytes) into NVmem logical space
 * @param size: Number of bytes to write
 * @param data: Pointer to source buffer
 * @param user: Data section within NvMem space
 * @return EC_ERROR_OVERFLOW if write exceeds buffer length
 *         EC_ERROR_TIMEOUT if nvmem cache buffer is not available
 *         EC_SUCCESS if no errors.
 */
int nvmem_write(unsigned int startOffset, unsigned int size,
		 void *data, enum nvmem_users user);


/**
 * Commit all previous NvMem writes to flash
 *
 * @return EC_SUCCESS if flash erase/operations are successful.
 *         EC_ERROR_UNKNOWN otherwise.
 */
int nvmem_commit(void);

/**
 * Configure both NvMem partitions
 *
 * @param version: starting version number for partition 0
 */
int nvmem_setup(uint16_t version);

/**
 * Get pointer to array of buffer lengths and number of users
 */
void nvmem_get_buffer_array(int32_t **p_buffers, int *p_num_buffers);

/**
 * Compute sha1 (lower 4 bytes) for NvMem tag
 */
void nvmem_compute_sha(uint8_t *p_buf, int num_bytes, uint8_t *p_sha);

#endif /* __CROS_EC_NVMEM_UTILS_H */
