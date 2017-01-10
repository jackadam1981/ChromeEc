/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"

#include "cryptoc/util.h"

static const char * const dcrypto_app_names[] = {
	"NVMEM"
};

int DCRYPTO_appkey_init(enum dcrypto_appid appid, struct APPKEY_CTX *ctx)
{
	LITE_SHA256_CTX hash_ctx;
	LITE_HMAC_CTX hmac_ctx;

	if (appid >= ARRAY_SIZE(dcrypto_app_names))
		return 0;

	memset(ctx, 0, sizeof(ctx));

	DCRYPTO_ladder_init();

	if (!DCRYPTO_ladder_compute_frk2(0, ctx->key))
		return 0;

	HMAC_SHA256_init(&hmac_ctx, ctx->key, sizeof(ctx->key));
	HMAC_update(&hmac_ctx, dcrypto_app_names[appid],
		sizeof(dcrypto_app_names[appid]));
	memcpy(ctx->key, HMAC_final(&hmac_ctx), SHA256_DIGEST_SIZE);

	/* Compute the key fingerprint. */
	DCRYPTO_SHA256_init(&hash_ctx, 0);
	HASH_update(&hash_ctx, ctx->key, sizeof(ctx->key));
	memcpy(&ctx->fingerprint, HASH_final(&hash_ctx),
		sizeof(ctx->fingerprint));

	return 1;
}

int DCRYPTO_appkey_fingerprint(struct APPKEY_CTX *ctx)
{
	return ctx->fingerprint;
}

int DCRYPTO_appkey_cipher_block(
	struct APPKEY_CTX *ctx, uint8_t *out, const uint8_t *in, size_t index)
{
	uint32_t ctr[4] = {0, 0, 0, 0};

	ctr[0] = htobe32(index);
	return DCRYPTO_aes_ctr(out, ctx->key, sizeof(ctx->key) * 8,
			(uint8_t *) ctr, in, 16);
}

void DCRYPTO_appkey_finish(struct APPKEY_CTX *ctx)
{
	always_memset(ctx, 0, sizeof(struct APPKEY_CTX));
	GREG32(KEYMGR, AES_WIPE_SECRETS) = 1;
}
