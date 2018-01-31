/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Provides the minimal declarations needed by pinweaver to build on
 * CHIP_HOST.
 */

#ifndef __CROS_EC_DCRYPTO_MOCKS_H
#define __CROS_EC_DCRYPTO_MOCKS_H

#include <stdint.h>
#include <string.h>

#define AES256_BLOCK_CIPHER_KEY_SIZE 32
#define SHA256_DIGEST_SIZE 32

extern pw_timestamp_t MOCK_update_timestamp;

struct HASH_CTX {
	uint8_t digest[SHA256_DIGEST_SIZE];
};

typedef struct {
	struct HASH_CTX hash;
} LITE_HMAC_CTX;

void HASH_update(struct HASH_CTX *ctx, const void *data, size_t len);

const uint8_t *DCRYPTO_SHA256_hash(const void *data, uint32_t n,
				   uint8_t *digest);

void DCRYPTO_HMAC_SHA256_init(LITE_HMAC_CTX *ctx, const void *key,
			      unsigned int len);
const uint8_t *DCRYPTO_HMAC_final(LITE_HMAC_CTX *ctx);

int DCRYPTO_aes_ctr(uint8_t *out, const uint8_t *key, uint32_t key_bits,
		    const uint8_t *iv, const uint8_t *in, size_t in_len);

#endif  /* __CROS_EC_DCRYPTO_MOCKS_H */
