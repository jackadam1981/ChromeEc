/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "cryptoc/util.h"
#include "dcrypto.h"
#include "internal.h"
#include "trng.h"

/* V = HMAC(K, V) */
static void update_v(const uint32_t *k, uint32_t *v)
{
	LITE_HMAC_CTX ctx;

	DCRYPTO_HMAC_SHA256_init(&ctx, k, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, v, SHA256_DIGEST_SIZE);
	memcpy(v, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);
}

/* K = HMAC(K, V || tag || p0 || p1 || p2) */
/* V = HMAC(K, V) */
static void update_kv(uint32_t *k, uint32_t *v, uint8_t tag,
		      const void *p0, size_t p0_len,
		      const void *p1, size_t p1_len,
		      const void *p2, size_t p2_len)
{
	LITE_HMAC_CTX ctx;

	DCRYPTO_HMAC_SHA256_init(&ctx, k, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, v, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, &tag, 1);
	HASH_update(&ctx.hash, p0, p0_len);
	HASH_update(&ctx.hash, p1, p1_len);
	HASH_update(&ctx.hash, p2, p2_len);
	memcpy(k, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);

	update_v(k, v);
}

static void update(struct drbg_ctx *ctx,
		   const void *p0, size_t p0_len,
		   const void *p1, size_t p1_len,
		   const void *p2, size_t p2_len)
{
	/* K = HMAC(K, V || 0x00 || provided_data) */
	/* V = HMAC(K, V) */
	update_kv(ctx->k, ctx->v, 0x00,
		  p0, p0_len, p1, p1_len, p2, p2_len);

	/* If no provided_data, stop. */
	if (p0_len + p1_len + p2_len == 0)
		return;

	/* K = HMAC(K, V || 0x01 || provided_data) */
	/* V = HMAC(K, V) */
	update_kv(ctx->k, ctx->v,
		  0x01,
		  p0, p0_len, p1, p1_len, p2, p2_len);
}

void hmac_drbg_init(struct drbg_ctx *ctx,
		    const void *p0, size_t p0_len,
		    const void *p1, size_t p1_len,
		    const void *p2, size_t p2_len)
{
	/* K = 0x00 0x00 0x00 ... 0x00 */
	always_memset(ctx->k,  0x00, sizeof(ctx->k));
	/* V = 0x01 0x01 0x01 ... 0x01 */
	always_memset(ctx->v,  0x01, sizeof(ctx->v));

	update(ctx, p0, p0_len, p1, p1_len, p2, p2_len);

	ctx->reseed_counter = 1;
}

void hmac_drbg_reseed(struct drbg_ctx *ctx,
		      const void *p0, size_t p0_len,
		      const void *p1, size_t p1_len,
		      const void *p2, size_t p2_len)
{
	update(ctx, p0, p0_len, p1, p1_len, p2, p2_len);
	ctx->reseed_counter = 1;
}

int hmac_drbg_generate(struct drbg_ctx *ctx,
		       void *out, size_t out_len,
		       const void *input, size_t input_len)
{
	/* TODO(louiscollard): Assert maximum output length? */

	if (ctx->reseed_counter >= 10000)
		return 2;

	if (input_len)
		update(ctx, input, input_len, NULL, 0, NULL, 0);

	while (out_len) {
		size_t n = out_len > sizeof(ctx->v) ? sizeof(ctx->v) : out_len;

		update_v(ctx->k, ctx->v);

		memcpy(out, ctx->v, n);
		out += n;
		out_len -= n;
	}

	update(ctx, input, input_len, NULL, 0, NULL, 0);
	ctx->reseed_counter++;

	return 0;
}
