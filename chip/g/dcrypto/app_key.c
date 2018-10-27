/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"

#include "cryptoc/util.h"

#include "console.h"

const char *const dcrypto_app_names[] = {
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
	size_t x;

	/* The PERSO_AUTH digest was improperly defined, so now this exception
	 * exists to prevent data loss.
	 */
	if (appid == PERSO_AUTH) {
		digest[0] = 0x2019da34;
		digest[1] = 0xf1a01a13;
		digest[2] = 0x0fb9f73f;
		digest[3] = 0xf2e85f76;
		digest[4] = 0x5ecb7690;
		digest[5] = 0x09f732c9;
		digest[6] = 0xe540bf14;
		digest[7] = 0xcc46799a;
		return;
	}

	DCRYPTO_SHA256_init(&ctx, 0);
	HASH_update(&ctx, name, strlen(name));
	memcpy(digest, HASH_final(&ctx), SHA256_DIGEST_SIZE);

	/* The digests were originally endian swapped because xxd was used to
	 * print them so this operation is needed to keep the derived keys the
	 * same. Any changes to they key derivation process must result in the
	 * same keys being produced given the same inputs, or devices will
	 * effectively be reset and user data will be lost by the key change.
	 */
	for (x = 0; x < SHA256_DIGEST_WORDS; ++x)
		digest[x] = __builtin_bswap32(digest[x]);
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

#ifdef CR50_DEV
static int test_keyladder_revocation(int argc, char *argv[])
{
	uint32_t app_id = 0;
	uint32_t digest[SHA256_DIGEST_WORDS];
	uint32_t in[SHA256_DIGEST_WORDS] = {0,};
	uint32_t out1[SHA256_DIGEST_WORDS] = {0,};
	uint32_t out2[SHA256_DIGEST_WORDS] = {0,};
	uint32_t out3[SHA256_DIGEST_WORDS] = {0,};

	if (argc > 1)
		app_id = atoi(argv[1]);

	name_hash(app_id, digest);

	ccprintf("KEYMGR_CERT_REVOKE_CTRL: %.10h\n",
			GREG32_ADDR(KEYMGR, CERT_REVOKE_CTRL0));
	ccprintf("\n");
	ccprintf("digest              %.32h\n", digest);
	ccprintf("in                  %.32h\n", in);

	/**/
	if (!DCRYPTO_app_cipher(app_id, digest, out1, in, sizeof(out1))) {
		ccprintf("%s app_cipher %d\n", __func__, __LINE__);
		return EC_ERROR_UNKNOWN;
	}
	ccprintf("cipher(in)  ->out1  %.32h\n", out1);

	if (!DCRYPTO_app_cipher(app_id, digest, out2, out1, sizeof(out2))) {
		ccprintf("%s app_cipher %d\n", __func__, __LINE__);
		return EC_ERROR_UNKNOWN;
	}
	ccprintf("cipher(out1)->out2  %.32h\n", out2);

	if (!DCRYPTO_equals(in, out2, sizeof(out1))) {
		ccprintf("two results are different. %s %d\n",
			__func__, __LINE__);
		return EC_ERROR_UNKNOWN;
	}

	ccprintf("\n");

	DCRYPTO_ladder_revoke();

	ccprintf("KEYMGR_CERT_REVOKE_CTRL: %.10h\n",
			GREG32_ADDR(KEYMGR, CERT_REVOKE_CTRL0));

	if (!DCRYPTO_app_cipher(app_id, digest, out3, out1, sizeof(out3))) {
		ccprintf("%s app_cipher %d\n", __func__, __LINE__);
		return EC_ERROR_UNKNOWN;
	}
	ccprintf("cipher(out1)->out3  %.32h\n", out3);

	if (DCRYPTO_equals(out2, out3, sizeof(out3))) {
		ccprintf("out2 and out3 are same, not expected. %s %d\n",
			 __func__, __LINE__);
		return EC_ERROR_UNKNOWN;
	}

	ccprintf("\n");

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(testc, test_keyladder_revocation,
			NULL,
			"Test Keyladder Revocation");
#endif

