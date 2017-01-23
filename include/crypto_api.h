/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __INCLUDE_CRYPTO_API_H
#define __INCLUDE_CRYPTO_API_H

#include "util.h"

/**
 * Compute sha1 (lower 4 bytes or equivalent checksum) for NvMem tag
 *
 * @param p_buf: pointer to beginning of data
 * @param num_bytes: length of data in bytes
 * @param p_sha: pointer to where computed sha will be stored
 * @param sha_len: length in bytes to use from sha computation
 */
void compute_hash(uint8_t *p_buf, int num_bytes,
		  uint8_t *p_hash, int hash_len);

#define CIPHER_SALT_SIZE 16

/*
 * Encrypt/decrypt a flat blob.
 *
 * Encrypt or decrypt the input buffer, and write the correspondingly
 * ciphered output to out.  The number of bytes produced is equal to
 * the number of input bytes.
 *
 * This API is expected to be applied to a single contiguous region. WARNING:
 * Presently calling this function more than once with "in" pointing to
 * logically different buffers will result in using the same IV value
 * internally and as such reduce encryption efficiency. Upcoming changes are
 * expected to make proper use of blob_iv.
 *
 * @param salt pointer to a unique value to be associated with this blob,
 *	       used for derivation of the proper IV, the size of the value
 *	       is as defined by CIPHER_SALT_SIZE above.
 * @param out Destination pointer where to write plaintext / ciphertext.
 * @param in  Source pointer where to read ciphertext / plaintext.
 * @param len Number of bytes to read from in / write to out.
 * @return non-zero on success, and zero otherwise.
 */
int app_cipher(const void *salt, void *out, const void *in, size_t size);

#endif /* __INCLUDE_CRYPTO_API_H */
