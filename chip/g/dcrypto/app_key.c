/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"

#include "cryptoc/util.h"

const char *dcrypto_app_names[] = {
	"RESERVED",
	"NVMEM",
	"U2F_ATTEST",
	"U2F_ORIGIN",
	"U2F_WRAP",
	/* This key signs data from H1's configured by mn50/scribe. */
	"PERSO_AUTH",
	"PINWEAVER",
};

static void name_hash(enum dcrypto_appid appid,
		      uint32_t digest[SHA256_DIGEST_WORDS])
{
	LITE_SHA256_CTX ctx;
	const char *name = dcrypto_app_names[appid];

	DCRYPTO_SHA256_init(&ctx, 0);
	HASH_update(&ctx, name, strlen(name));
	memcpy(digest, HASH_final(&ctx), SHA256_DIGEST_SIZE);
}

int DCRYPTO_appkey_init(enum dcrypto_appid appid, struct APPKEY_CTX *ctx)
{
	uint32_t digest[SHA256_DIGEST_WORDS];

	memset(ctx, 0, sizeof(*ctx));
	name_hash(appid, digest);

	if (!dcrypto_ladder_compute_usr(appid, digest))
		return 0;

	return 1;
}

void DCRYPTO_appkey_finish(struct APPKEY_CTX *ctx)
{
	always_memset(ctx, 0, sizeof(struct APPKEY_CTX));
	GREG32(KEYMGR, AES_WIPE_SECRETS) = 1;
}

int DCRYPTO_appkey_derive(enum dcrypto_appid appid, const uint32_t input[8],
			  uint32_t output[8])
{
	uint32_t digest[SHA256_DIGEST_WORDS];

	name_hash(appid, digest);
	return !!dcrypto_ladder_derive(appid, digest, input, output);
}
