/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_NVMEM_UTILS_H
#define __CROS_EC_NVMEM_UTILS_H

#define NVMEM_SHA_SIZE 4
#define NVMEM_VERSION_BITS 8
#define NVMEM_VERSION_MASK ((1 << NVMEM_VERSION_BITS) - 1)
/* Struct for NV block tag */
struct nvmem_tag {
	uint8_t sha[NVMEM_SHA_SIZE];
	uint8_t version;
	uint8_t reserved[3];
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

int nvmem_setup(uint16_t version);
void nvmem_set_dump_level(int size);

/**
 * Get pointer to array of user buffer lengths and number of users
 *
 * @param p_user_buffers: pointer to array of user buffer lengths
 * @param p_num_users: pointer to return number of user buffers
 */
void nvmem_get_users(int32_t **p_user_buffers, int *p_num_users);

/**
 * Compute sha1 (lower 4 bytes) for NvMem tag
 *
 * @param p_buf: pointer to beginning of partition
 * @param num_bytes: length of partition in bytes
 * @param p_sha: pointer to where computed sha will be stored
 * @param sha_len: length in bytes to use from sha computation
 */
void nvmem_compute_sha(uint8_t *p_buf, int num_bytes, uint8_t *p_sha,
		       int sha_len);

#endif /* __CROS_EC_NVMEM_UTILS_H */
