/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"

void DCRYPTO_SHA256_init(struct sha256_ctx *ctx, uint32_t sw_required)
{
	SHA256_sw_init(ctx);
}

const uint8_t *DCRYPTO_SHA256_hash(const void *data, size_t n, uint8_t *digest)
{
	SHA256_sw_hash(data, n, digest);
	return digest;
}

void DCRYPTO_HMAC_SHA256_init(struct hmac_sha256_ctx *ctx, const void *key,
			      size_t len)
{
	DCRYPTO_SHA256_init(&ctx->hash, 0);
	HMAC_sw_init((union hmac_ctx *)ctx, key, len);
}

const uint8_t *DCRYPTO_HMAC_final(struct hmac_sha256_ctx *ctx)
{
	return HMAC_SHA256_final(ctx);
}
