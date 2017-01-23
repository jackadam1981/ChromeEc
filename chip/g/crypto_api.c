/*
 * Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crypto_api.h"
#include "dcrypto.h"

void compute_hash(uint8_t *p_buf, int num_bytes,
		  uint8_t *p_hash, int hash_len)
{
	uint8_t sha1_digest[SHA_DIGEST_SIZE];
	/*
	 * Taking advantage of the built in dcrypto engine to generate
	 * a CRC-like value that can be used to validate contents of an
	 * NvMem partition. Only using the lower 4 bytes of the sha1 hash.
	 */
	DCRYPTO_SHA1_hash((uint8_t *)p_buf,
			  num_bytes,
			  sha1_digest);
	memcpy(p_hash, sha1_digest, hash_len);
}

int app_cipher(const void *salt, void *out, const void *in, size_t size)
{
	return DCRYPTO_app_cipher(salt, out, in, size);
}
