/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "dice.h"
#include "platform.h"
#include "cbor_dice.h"

#define DBGDUMP(name)                        \
	do {                                 \
		printf("%s: ", #name);       \
		hexdump(name, sizeof(name)); \
		printf("\n");                \
	} while (0)

static void hexdump(const uint8_t *buf, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		printf("%02x ", buf[i]);
}

static void test_hkdf(void)
{
	uint8_t derived[DIGEST_BYTES];
	const struct slice_ref_s ikm = {
		3, (const uint8_t *)"key"
	};
	const struct slice_ref_s salt = {
		4, (const uint8_t *)"salt"
	};
	const struct slice_ref_s info = {
		5, (const uint8_t *)"label"
	};
	const struct slice_mut_s result = {DIGEST_BYTES, derived };

	if (__platform_hkdf_sha256(ikm, salt, info, result)) {
		__platform_log_str("HKDF: success");
		DBGDUMP(derived);
	} else {
		__platform_log_str("HKDF: failed");
	}
}

static void test_sha(void)
{
	uint8_t digest[DIGEST_BYTES];
	const struct slice_ref_s input = {
		4, (const uint8_t *)"test"
	};

	if (__platform_sha256(input, digest)) {
		__platform_log_str("SHA256: success");
		DBGDUMP(digest);
	} else {
		__platform_log_str("SHA256: failed");
	}
}

static void test_ecdsa(void)
{
	uint8_t seed[DIGEST_BYTES] = { 0 };
	const void *key;
	const struct slice_ref_s input = {
		4, (const uint8_t *)"test"
	};
	uint8_t signature[ECDSA_SIG_BYTES];
	struct ecdsa_public_s pub_key;

	if (!__platform_ecdsa_p256_keygen_hmac_drbg(seed, &key)) {
		__platform_log_str("ECDSA: keygen failed");
		return;
	}

	if (!__platform_ecdsa_p256_sign(key, input, signature)) {
		__platform_log_str("ECDSA: sign failed");
		__platform_ecdsa_p256_free(key);
		return;
	}

	if (!__platform_ecdsa_p256_get_pub_key(key, &pub_key)) {
		__platform_log_str("ECDSA: get pubkey failed");
		__platform_ecdsa_p256_free(key);
		return;
	}

	__platform_ecdsa_p256_free(key);
	__platform_log_str("ECDSA: success");
	DBGDUMP(signature);
	DBGDUMP(pub_key.x);
	DBGDUMP(pub_key.y);
}

int main(void)
{
	uint8_t cbor_hdr[] = CFG_DESCR_LABEL_RESETTABLE;

	DBGDUMP(cbor_hdr);

	printf("kDiceHandoverSize = %zu\n", kDiceHandoverSize);

	test_hkdf();
	test_sha();
	test_ecdsa();

	return 0;
}
