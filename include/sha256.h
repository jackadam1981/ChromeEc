/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SHA-256 functions */

#ifndef __CROS_EC_SHA256_H
#define __CROS_EC_SHA256_H

#include "common.h"

#define SHA256_DIGEST_SIZE 32
#define SHA256_BLOCK_SIZE 64

/*
 * SHA256 context
 * Some SHA hardware accelerators require aligned data.
 * Make sure block and buf are aligned >= 4 bytes
 * and provide 8 and 32 bit access.
 */
struct sha256_ctx {
	uint32_t h[8];
	uint32_t tot_len;
	uint32_t len;

	/* block and buf aligned on >= 4 byte boundary */
	union {
		uint8_t  block[2 * SHA256_BLOCK_SIZE];
		uint32_t wblock[(2 * SHA256_BLOCK_SIZE) / 4];
	};

	union {
		uint8_t  buf[SHA256_DIGEST_SIZE];
		uint32_t wbuf[SHA256_DIGEST_SIZE / 4];
	}; /* Used to store the final digest. */
};

void SHA256_init(struct sha256_ctx *ctx);
void SHA256_update(struct sha256_ctx *ctx, const uint8_t *data, uint32_t len);
uint8_t *SHA256_final(struct sha256_ctx *ctx);

void hmac_SHA256(uint8_t *output, const uint8_t *key, const int key_len,
		 const uint8_t *message, const int message_len);

#endif  /* __CROS_EC_SHA256_H */
