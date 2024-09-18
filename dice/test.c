/*
 * Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "dice.h"
#include "platform.h"
#include "cbor_dice.h"

#undef TEST_PLATFORM

#ifdef TEST_PLATFORM

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
#endif /* TEST_PLATFORM */

static bool save_to_file(
	const char *filename,
	uint8_t dice_handover[kDiceHandoverSize]
)
{
	FILE *f;
	size_t written;

	f = fopen(filename, "wb");
	if (f == NULL) {
		printf("Failed to open file \"%s\"\n", filename);
		return false;
	}
	written = fwrite(dice_handover, 1, kDiceHandoverSize, f);
	fclose(f);
	return written == kDiceHandoverSize;
}

static bool test_dice_handover(const char *filename)
{
	uint8_t dice_handover[kDiceHandoverSize];
	size_t res =
		get_dice_handover_bytes(dice_handover, 0, kDiceHandoverSize);

	if (res != kDiceHandoverSize) {
		printf("get_dice_handover_bytes failed");
		return false;
	}
	return save_to_file(filename, dice_handover);
}

int main(int argc, char *argv[])
{
#ifdef TEST_PLATFORM
	printf("kDiceHandoverSize = %zu\n", kDiceHandoverSize);
	test_hkdf();
	test_sha();
	test_ecdsa();
#endif /* TEST_PLATFORM */

	if (argc != 2) {
		printf("Syntax: %s <filename>\n", argv[0]);
		return 2;
	}
	return test_dice_handover(argv[1]) ? 0 : 1;
}
