/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "builtin/endian.h"
#include "console.h"
#include "dcrypto.h"
#include "ec_commands.h"
#include "extension.h"
#include "fips.h"
#include "fips_rand.h"
#include "flash_log.h"
#include "hooks.h"
#include "new_nvmem.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "registers.h"
#include "scratch_reg1.h"
#include "shared_mem.h"
#include "system.h"
#include "tpm_nvmem_ops.h"
#include "u2f_impl.h"

/**
 * Combined FIPS status & global error.
 * default value is  = FIPS_UNINITIALIZED
 */
static enum fips_status _fips_status;

/* return current FIPS status, but prevent direct modification of state */
enum fips_status fips_status(void)
{
	return _fips_status;
}

/* flag to simulate specific error condition in power-up tests */
uint8_t fips_break_cmd;

void fips_set_status(enum fips_status status)
{
	/**
	 * if FIPS error took place, drop indication of FIPS approved mode.
	 * Next cycle of sleep will power-cycle HW crypto components, so any
	 * soft-errors will be recovered. In case of hard errors it
	 * will be detected again.
	 */
	/* accumulate status */
	_fips_status |= status;

	status = _fips_status;
	/* if we have error, require power up tests on resume */
	if (status & FIPS_ERROR_MASK)
		board_set_fips_policy_test(false);
}

bool fips_mode(void)
{
	return (_fips_status & FIPS_MODE_ACTIVE);
}

static const uint8_t k_salt = NVMEM_VAR_G2F_SALT;


/* we can't include TPM2 headers, so just define constant locally */
#define HR_NV_INDEX (1U << 24)

static void u2f_zeroize(void)
{
	const uint32_t u2fobjs[] = { TPM_HIDDEN_U2F_KEK | HR_NV_INDEX,
					     TPM_HIDDEN_U2F_KH_SALT |
	   HR_NV_INDEX, 0 };
	/* delete now */
	setvar(&k_salt, sizeof(k_salt), NULL, 0);
	/* remove and wipe all deleted objects */
	nvmem_erase_tpm_data_selective(u2fobjs);
}

/* return false if U2F keys require zeroization, true otherwise */
static bool fips_u2f_compliant(void)
{
	uint8_t val_len = 0;
	const struct tuple *t_salt = getvar(&k_salt, sizeof(k_salt));

	if (t_salt) {
		val_len = t_salt->val_len;
		freevar(t_salt);
	}

	/* old, non FIPS compliant U2F keys will all have size of 32 bytes */
	return !((val_len == 32) &&
		 (read_tpm_nvmem_size(TPM_HIDDEN_U2F_KEK) == 32) &&
		 (read_tpm_nvmem_size(TPM_HIDDEN_U2F_KH_SALT) == 32));
}

/* Return true if crypto can be used (no failures detectd) */
bool fips_crypto_allowed(void)
{
	/**
	 * We never allow crypto if there were errors, no matter
	 * if we are in FIPS approved or not-approved mode.
	 */
	return ((_fips_status & FIPS_POWER_UP_TEST_DONE) &&
		!(_fips_status & FIPS_ERROR_MASK));
}

void fips_throw_err(enum fips_status err)
{
	/* if not a new error, don't do anything */
	if ((_fips_status & err) == err)
		return;
	fips_set_status(err);
	if (_fips_status & FIPS_ERROR_MASK) {
		flash_log_add_event(FE_LOG_FIPS_FAILURE, sizeof(_fips_status),
				    &_fips_status);
	}
}

static void *kat_buf;  /* pointer to scratch buffer for KAT */

struct sha256_kat {
	struct HASH_CTX ctx;
};

/* Test values from OpenSSL */
static bool fips_sha256_kat(void)
{
	struct sha256_kat *b = kat_buf;

	static const uint8_t in[] = /* "etaonrishd" */ { 0x65, 0x74, 0x61, 0x6f,
							 0x6e, 0x72, 0x69, 0x73,
							 0x68, 0x64 };
	static const uint8_t ans[] = { 0xf5, 0x53, 0xcd, 0xb8, 0xcf, 0x1,  0xee,
				       0x17, 0x9b, 0x93, 0xc9, 0x68, 0xc0, 0xea,
				       0x40, 0x91, 0x6,	 0xec, 0x8e, 0x11, 0x96,
				       0xc8, 0x5d, 0x1c, 0xaf, 0x64, 0x22, 0xe6,
				       0x50, 0x4f, 0x47, 0x57 };

	DCRYPTO_SHA256_init(&b->ctx, 0);
	HASH_update(&b->ctx, in, sizeof(in));
	return !(fips_break_cmd == FIPS_BREAK_SHA256) &&
	       (memcmp(HASH_final(&b->ctx), ans, SHA256_DIGEST_SIZE) == 0);
}

struct hmac_kat {
	LITE_HMAC_CTX ctx;
};

/* Test values from OpenSSL */
static bool fips_hmac_sha256_kat(void)
{
	struct hmac_kat *b = kat_buf;

	static const uint8_t k[SHA256_DIGEST_SIZE] =
		/* "etaonrishd" */ { 0x65, 0x74, 0x61, 0x6f, 0x6e, 0x72, 0x69,
				     0x73, 0x68, 0x64, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00 };
	static const uint8_t in[] =
		/* "Sample text" */ { 0x53, 0x61, 0x6d, 0x70, 0x6c, 0x65,
				      0x20, 0x74, 0x65, 0x78, 0x74 };
	static const uint8_t ans[] = { 0xe9, 0x17, 0xc1, 0x7b, 0x4c, 0x6b, 0x77,
				       0xda, 0xd2, 0x30, 0x36, 0x02, 0xf5, 0x72,
				       0x33, 0x87, 0x9f, 0xc6, 0x6e, 0x7b, 0x7e,
				       0xa8, 0xea, 0xaa, 0x9f, 0xba, 0xee, 0x51,
				       0xff, 0xda, 0x24, 0xf4 };

	DCRYPTO_HMAC_SHA256_init(&b->ctx, k, sizeof(k));
	HASH_update(&b->ctx.hash, in, sizeof(in));
	return !(fips_break_cmd == FIPS_BREAK_HMAC_SHA256) &&
	       (memcmp(DCRYPTO_HMAC_final(&b->ctx), ans, SHA256_DIGEST_SIZE) ==
		0);
}

static const uint8_t drbg_entropy0[] = {
	0x42, 0x94, 0x67, 0x1d, 0x49, 0x3d, 0xc0, 0x85, 0xb5, 0x18, 0x46,
	0x07, 0xd7, 0xde, 0x2f, 0xf2, 0xb6, 0xac, 0xeb, 0x73, 0x4a, 0x1b,
	0x02, 0x6f, 0x6c, 0xfe, 0xe7, 0xc5, 0xa9, 0x0f, 0x03, 0xda
};
static const uint8_t drbg_nonce0[] = { 0xd0, 0x71, 0x54, 0x4e, 0x59, 0x92,
				       0x35, 0xd5, 0xeb, 0x38, 0xb6, 0x4b,
				       0x55, 0x1d, 0x2a, 0x6e };
static const uint8_t drbg_perso0[] = { 0x63, 0xbc, 0x76, 0x9a, 0xe1, 0xd9, 0x5a,
				       0x98, 0xbd, 0xe8, 0x70, 0xe4, 0xdb, 0x77,
				       0x76, 0x29, 0x70, 0x41, 0xd3, 0x7c, 0x8a,
				       0x5c, 0x68, 0x8d, 0x4e, 0x02, 0x4b, 0x78,
				       0xd8, 0x3f, 0x4d, 0x78 };

static const uint8_t drbg_entropy1[] = {
	0xdb, 0x9b, 0x47, 0x90, 0xb6, 0x23, 0x36, 0xfb, 0xb9, 0xa6, 0x84,
	0xb8, 0x29, 0x47, 0x06, 0x53, 0x93, 0xee, 0xef, 0x8f, 0x57, 0xbd,
	0x24, 0x77, 0x14, 0x1a, 0xd1, 0x7e, 0x77, 0x6d, 0xac, 0x34
};
static const uint8_t drbg_addtl_input1[] = {
	0x28, 0x84, 0x8b, 0xec, 0xd3, 0xf4, 0x76, 0x96, 0xf1, 0x24, 0xf4,
	0xb1, 0x48, 0x53, 0xa4, 0x56, 0x15, 0x6f, 0x69, 0xbe, 0x58, 0x3a,
	0x7d, 0x46, 0x82, 0xcf, 0xf8, 0xd4, 0x4b, 0x39, 0xe1, 0xd3
};

static const uint8_t drbg_entropy2[] = {
	0x4a, 0x9a, 0xbe, 0x80, 0xf6, 0xf5, 0x22, 0xf2, 0x98, 0x78, 0xbe,
	0xdf, 0x82, 0x45, 0xb2, 0x79, 0x40, 0xa7, 0x64, 0x71, 0x00, 0x6f,
	0xb4, 0xa4, 0x11, 0x0b, 0xeb, 0x4d, 0xec, 0xb6, 0xc3, 0x41
};
static const uint8_t drbg_addtl_input2[] = {
	0x8b, 0xfc, 0xe0, 0xb7, 0x13, 0x26, 0x61, 0xc3, 0xcd, 0x78, 0x17,
	0x5d, 0x83, 0x92, 0x6f, 0x64, 0x3e, 0x36, 0xf7, 0x60, 0x8e, 0xec,
	0x2c, 0x5d, 0xac, 0x3d, 0xdc, 0xba, 0xcc, 0x8c, 0x21, 0x82
};

/**
 *  DRBG test vector source recorded 6/1/17 from
 * http://csrc.nist.gov/groups/STM/cavp/documents/drbg/drbgtestvectors.zip,
 * Input values:
 * [SHA-256]
 * [PredictionResistance = True]
 * [EntropyInputLen = 256]
 * [NonceLen = 128]
 * [PersonalizationStringLen = 256]
 * [AdditionalInputLen = 256]
 * [ReturnedBitsLen = 1024]
 * COUNT = 0
 * EntropyInput =
 * 4294671d493dc085b5184607d7de2ff2b6aceb734a1b026f6cfee7c5a90f03da
 * Nonce = d071544e599235d5eb38b64b551d2a6e
 * PersonalizationString =
 * 63bc769ae1d95a98bde870e4db7776297041d37c8a5c688d4e024b78d83f4d78
 * AdditionalInput =
 * 28848becd3f47696f124f4b14853a456156f69be583a7d4682cff8d44b39e1d3
 * EntropyInputPR =
 * db9b4790b62336fbb9a684b82947065393eeef8f57bd2477141ad17e776dac34
 * AdditionalInput =
 * 8bfce0b7132661c3cd78175d83926f643e36f7608eec2c5dac3ddcbacc8c2182
 * EntropyInputPR =
 * 4a9abe80f6f522f29878bedf8245b27940a76471006fb4a4110beb4decb6c341
 * ReturnedBits =
 * e580dc969194b2b18a97478aef9d1a72390aff14562747bf080d741527a6655
 * ce7fc135325b457483a9f9c70f91165a811cf4524b50d51199a0df3bd60d12abac27d0bf6618
 * e6b114e05420352e23f3603dfe8a225dc19b3d1fff1dc245dc6b1df24c741744bec3f9437dbb
 * f222df84881a457a589e7815ef132f686b760f012

 * DRBG KAT generation sequence:
 * hmac_drbg_init(entropy0, nonce0, perso0)
 * hmac_drbg_reseed(entropy1, addtl_input1)
 * hmac_drbg_generate()
 * hmac_drbg_reseed(entropy2, addtl_input2)
 * hmac_drbg_generate()
 */
struct hmac_drbg_kat {
	struct drbg_ctx ctx;
	uint8_t buf[128];
};

static bool fips_hmac_drbg_instantiate_kat(struct hmac_drbg_kat *b)
{
	/* Expected internal drbg state */
	static const uint32_t K0[] = { 0x7fe2b43a, 0x94f11b33, 0x2b76c5ce,
				       0xfbb784af, 0x81cfe716, 0xc43596d6,
				       0xbdfe968b, 0x189c80fb };
	static const uint32_t V0[] = { 0xc42b237a, 0x929cdd0b, 0xe7fbafdd,
				       0xba22a36a, 0x4d23471a, 0xd8607022,
				       0x687e18ac, 0x2ac08007 };

	hmac_drbg_init(&b->ctx, drbg_entropy0, sizeof(drbg_entropy0),
		       drbg_nonce0, sizeof(drbg_nonce0), drbg_perso0,
		       sizeof(drbg_perso0));

	return (memcmp(b->ctx.v, V0, sizeof(V0)) == 0) &&
	       (memcmp(b->ctx.k, K0, sizeof(K0)) == 0);
}

static bool fips_hmac_drbg_reseed_kat(struct hmac_drbg_kat *b)
{
	/* Expected internal drbg state */
	static const uint32_t K1[] = { 0x3118D36E, 0x05DEEC48, 0x7EFB6363,
				       0x3D575CDE, 0xCFCD14C1, 0x8D4F937D,
				       0x896B811E, 0x0EF038EB };
	static const uint32_t V1[] = { 0xC8ED8EEC, 0x24DD7B66, 0x09C635CD,
				       0x6AC74196, 0xC70067D7, 0xC2E71FEF,
				       0x918D9EB7, 0xAE0CD544 };

	hmac_drbg_reseed(&b->ctx, drbg_entropy1, sizeof(drbg_entropy1),
			 drbg_addtl_input1, sizeof(drbg_addtl_input1), NULL, 0);

	return (memcmp(b->ctx.v, V1, sizeof(V1)) == 0) &&
	       (memcmp(b->ctx.k, K1, sizeof(K1)) == 0);
}

static bool fips_hmac_drbg_generate_kat(struct hmac_drbg_kat *b)
{
	/* Expected internal drbg state */
	static const uint32_t K2[] = { 0x980ccd6a, 0x0b34f7e1, 0x594aabd7,
				       0x33b66049, 0xb919bd57, 0x8ecc7194,
				       0xaf1748a3, 0x80982577 };
	static const uint32_t V2[] = { 0xe4927cdb, 0xb3435cc5, 0x601ab870,
				       0x46e1f024, 0x966ca875, 0x102b4167,
				       0xa71e5dce, 0xe4c15962 };
	/* Expected output */
	static const uint8_t KA[] = {
		0xe5, 0x80, 0xdc, 0x96, 0x91, 0x94, 0xb2, 0xb1, 0x8a, 0x97,
		0x47, 0x8a, 0xef, 0x9d, 0x1a, 0x72, 0x39, 0x0a, 0xff, 0x14,
		0x56, 0x27, 0x47, 0xbf, 0x08, 0x0d, 0x74, 0x15, 0x27, 0xa6,
		0x65, 0x5c, 0xe7, 0xfc, 0x13, 0x53, 0x25, 0xb4, 0x57, 0x48,
		0x3a, 0x9f, 0x9c, 0x70, 0xf9, 0x11, 0x65, 0xa8, 0x11, 0xcf,
		0x45, 0x24, 0xb5, 0x0d, 0x51, 0x19, 0x9a, 0x0d, 0xf3, 0xbd,
		0x60, 0xd1, 0x2a, 0xba, 0xc2, 0x7d, 0x0b, 0xf6, 0x61, 0x8e,
		0x6b, 0x11, 0x4e, 0x05, 0x42, 0x03, 0x52, 0xe2, 0x3f, 0x36,
		0x03, 0xdf, 0xe8, 0xa2, 0x25, 0xdc, 0x19, 0xb3, 0xd1, 0xff,
		0xf1, 0xdc, 0x24, 0x5d, 0xc6, 0xb1, 0xdf, 0x24, 0xc7, 0x41,
		0x74, 0x4b, 0xec, 0x3f, 0x94, 0x37, 0xdb, 0xbf, 0x22, 0x2d,
		0xf8, 0x48, 0x81, 0xa4, 0x57, 0xa5, 0x89, 0xe7, 0x81, 0x5e,
		0xf1, 0x32, 0xf6, 0x86, 0xb7, 0x60, 0xf0, 0x12
	};

	hmac_drbg_generate(&b->ctx, b->buf, sizeof(b->buf), NULL, 0);
	/* Verify internal drbg state */
	if (memcmp(b->ctx.v, V2, sizeof(V2)) ||
	    memcmp(b->ctx.k, K2, sizeof(K2))) {
		return false;
	}

	hmac_drbg_reseed(&b->ctx, drbg_entropy2, sizeof(drbg_entropy2),
			 drbg_addtl_input2, sizeof(drbg_addtl_input2), NULL, 0);
	/**
	 * reuse entropy buffer to avoid allocating too much stack and memory
	 * it will be cleaned up in TRNG health test
	 */
	hmac_drbg_generate(&b->ctx, b->buf, sizeof(b->buf), NULL, 0);
	return !(fips_break_cmd == FIPS_BREAK_HMAC_DRBG) &&
	       (memcmp(b->buf, KA, sizeof(KA)) == 0);
}

static bool fips_hmac_drbg_kat(void)
{
	struct hmac_drbg_kat *b = kat_buf;

	return fips_hmac_drbg_instantiate_kat(b) &&
	       fips_hmac_drbg_reseed_kat(b) && fips_hmac_drbg_generate_kat(b);
}

struct ecdsa_kat {
	p256_int p256_digest;
	uint8_t digest[SHA256_DIGEST_SIZE];
	uint8_t bad_msg[SHA256_DIGEST_SIZE];
};

static bool fips_ecdsa_verify_kat(void)
{
	struct ecdsa_kat *b = kat_buf;
	static const p256_int qx = { { 0xf49abf3c, 0xf82e6e12, 0x7a67c074,
				       0x5134e16f, 0xf8957a0c, 0xef4344a7,
				       0xd4bb3cb7, 0xe424dc61 } };
	static const p256_int qy = { { 0xdfaee927, 0x3d6f60e7, 0xac85d124,
				       0x127e5965, 0xe1dddaf0, 0x1545949d,
				       0xa2bc4865, 0x970eed7a } };
	static const p256_int r = { { 0xd9347f4f, 0xb72f981f, 0x6349b9da,
				      0x2ff540c7, 0x42017c64, 0x910be331,
				      0xa49c705c, 0xbf96b99a } };
	static const p256_int s = { { 0x57ec871c, 0x920b9e0f, 0x75d98f31,
				      0x444e3230, 0x15abdf12, 0xe03b9cd4,
				      0x819089c2, 0x17c55095 } };
	static const uint8_t msg[128] = {
		0xe1, 0x13, 0x0a, 0xf6, 0xa3, 0x8c, 0xcb, 0x41, 0x2a, 0x9c,
		0x8d, 0x13, 0xe1, 0x5d, 0xbf, 0xc9, 0xe6, 0x9a, 0x16, 0x38,
		0x5a, 0xf3, 0xc3, 0xf1, 0xe5, 0xda, 0x95, 0x4f, 0xd5, 0xe7,
		0xc4, 0x5f, 0xd7, 0x5e, 0x2b, 0x8c, 0x36, 0x69, 0x92, 0x28,
		0xe9, 0x28, 0x40, 0xc0, 0x56, 0x2f, 0xbf, 0x37, 0x72, 0xf0,
		0x7e, 0x17, 0xf1, 0xad, 0xd5, 0x65, 0x88, 0xdd, 0x45, 0xf7,
		0x45, 0x0e, 0x12, 0x17, 0xad, 0x23, 0x99, 0x22, 0xdd, 0x9c,
		0x32, 0x69, 0x5d, 0xc7, 0x1f, 0xf2, 0x42, 0x4c, 0xa0, 0xde,
		0xc1, 0x32, 0x1a, 0xa4, 0x70, 0x64, 0xa0, 0x44, 0xb7, 0xfe,
		0x3c, 0x2b, 0x97, 0xd0, 0x3c, 0xe4, 0x70, 0xa5, 0x92, 0x30,
		0x4c, 0x5e, 0xf2, 0x1e, 0xed, 0x9f, 0x93, 0xda, 0x56, 0xbb,
		0x23, 0x2d, 0x1e, 0xeb, 0x00, 0x35, 0xf9, 0xbf, 0x0d, 0xfa,
		0xfd, 0xcc, 0x46, 0x06, 0x27, 0x2b, 0x20, 0xa3
	};

	int passed;

	DCRYPTO_SHA256_hash(msg, sizeof(msg), b->digest);
	p256_from_bin(b->digest, &b->p256_digest);
	passed = dcrypto_p256_ecdsa_verify(&qx, &qy, &b->p256_digest, &r, &s);
	if (!passed)
		return false;
	/**
	 * create bad_msg same as msg but has one bit flipped in byte 92 (0x0a
	 * vs 0x1a) this is to save space in flash vs. having bad message as
	 * constant
	 */
	memcpy(b->bad_msg, msg, sizeof(msg));
	b->bad_msg[92] ^= 0x10;
	DCRYPTO_SHA256_hash(b->bad_msg, sizeof(b->bad_msg), b->digest);
	p256_from_bin(b->digest, &b->p256_digest);
	passed = dcrypto_p256_ecdsa_verify(&qx, &qy, &b->p256_digest, &r, &s);
	return !(fips_break_cmd == FIPS_BREAK_ECDSA) && (passed == 0);
}

#define AES_BLOCK_LEN 16
struct aes256_kat {
	uint8_t enc[AES_BLOCK_LEN];
	uint8_t dec[AES_BLOCK_LEN];
	uint8_t iv[AES_BLOCK_LEN];
};

static bool fips_aes256_kat(void)
{
	struct aes256_kat *b = kat_buf;
	static const uint8_t kat_aes128_k[AES256_BLOCK_CIPHER_KEY_SIZE] = {
		0x65, 0x74, 0x61, 0x6f, 0x6e, 0x72, 0x69, 0x73,
		0x68, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t kat_aes128_msg[AES_BLOCK_LEN] = {
		0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA,
		0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA, 0x00, 0xAA
	};

	static const uint8_t ans_aes128[AES_BLOCK_LEN] = {
		0x64, 0x62, 0x89, 0x41, 0x73, 0x63, 0x70, 0xe9,
		0x12, 0x7e, 0xa7, 0x1b, 0x1b, 0xc3, 0x57, 0x8d
	};

	memset(b->iv, 0, sizeof(b->iv));
	DCRYPTO_aes_init(kat_aes128_k, 256, b->iv, CIPHER_MODE_CBC,
			 ENCRYPT_MODE);
	DCRYPTO_aes_block(kat_aes128_msg, b->enc);
	if (memcmp(b->enc, ans_aes128, AES_BLOCK_LEN))
		return false;

	DCRYPTO_aes_init(kat_aes128_k, 256, b->iv, CIPHER_MODE_CBC,
			 DECRYPT_MODE);
	DCRYPTO_aes_block(b->enc, b->dec);

	return !(fips_break_cmd == FIPS_BREAK_AES256) &&
	       (memcmp(kat_aes128_msg, b->dec, AES_BLOCK_LEN) == 0);
}

/* dummy struct to find memory size to allocate for power-up tests */
union kat_tests {
	struct sha256_kat sha256;
	struct hmac_kat hmac;
	struct hmac_drbg_kat drbg;
	struct ecdsa_kat ecdsa;
	/*	struct rsa2048_kat rsa; */
	struct aes256_kat aes128;
};

struct kat_context {
	uint32_t stack[384];	/* stack */
	union kat_tests buf;	/* scratchpad buffer */
};

static bool call_on_stack(void *new_stack, bool (*func)(void))
{
	bool result;
	/* Call whilst switching stacks */
	__asm__ volatile("mov r4, sp\n" /* save sp */
			 "mov sp, %[new_stack]\n"
			 "blx %[func]\n"
			 "mov sp, r4\n" /* restore sp */
			 "mov %[result], r0\n"
			 : [result] "=r"(result)
			 : [new_stack] "r"(new_stack),
			   [func] "r"(func)
			 : "r0", "r1", "r2", "r3", "r4",
			   "lr" /* clobbers */
	);
	return result;
}


/**
 * Initialization
 * Single point of initialization for all FIPS-compliant
 * cryptography. Responsible for KATs, TRNG testing, and signalling a
 * fatal error.
 */
static uint64_t fips_power_up_tests(void)
{
	struct kat_context  *kat_ctx;
	void *stack;
	uint64_t starttime;

	starttime = get_time().val;
	/**
	 * since we are very limited on stack and static RAM, acquire
	 * shared memory for KAT tests temporary buffer
	 */
	if (EC_SUCCESS ==
	    shared_mem_acquire(sizeof(struct kat_context), (char **)&kat_ctx)) {
		kat_buf = &kat_ctx->buf;
		stack = ((char *)kat_ctx->stack) + sizeof(kat_ctx->stack);
		if (!call_on_stack(stack, &fips_sha256_kat))
			_fips_status |= FIPS_FATAL_SHA256;
		if (!call_on_stack(stack, &fips_hmac_sha256_kat))
			_fips_status |= FIPS_FATAL_HMAC_SHA256;
		/**
		 * Since TRNG FIFO takes some time to fill in, we can mask
		 * latency by splitting TRNG tests in 2 halves, each
		 * 2048 bits. This saves 20 ms on start.
		 * first call to TRNG warm-up
		 */
		fips_trng_startup(0);
		if (!call_on_stack(stack, &fips_ecdsa_verify_kat))
			_fips_status |= FIPS_FATAL_ECDSA;

		if (!call_on_stack(stack, &fips_aes256_kat))
			_fips_status |= FIPS_FATAL_AES256;
		if (!call_on_stack(stack, &fips_hmac_drbg_kat))
			_fips_status |= FIPS_FATAL_HMAC_DRBG;

		/* TODO: switch to a larger stack for rsa2048 */
#ifdef CR50_FIPS_RSA
		fips_rsa2048_verify_kat(kat_buf);
#endif
		/**
		 * Grab the SHA hardware lock to force the following KATs to use
		 * the SW implementation.
		 */
		if (!dcrypto_grab_sha_hw())
			_fips_status |= FIPS_FATAL_SHA256;

		if (!call_on_stack(stack, &fips_sha256_kat))
			_fips_status |= FIPS_FATAL_SHA256;
		if (!call_on_stack(stack, &fips_hmac_sha256_kat))
			_fips_status |= FIPS_FATAL_HMAC_SHA256;

		/*	TODO: increase stack to enable fips_hmac_drbg_kat(); */

		dcrypto_release_sha_hw();
		shared_mem_release((char *)kat_ctx);

		fips_trng_startup(1);
		/* if no errors, set not to run tests on wake from sleep */
		if (!(_fips_status & FIPS_ERROR_MASK))
			board_set_fips_policy_test(true);
		else
			flash_log_add_event(FE_LOG_FIPS_FAILURE,
					    sizeof(_fips_status),
					    &_fips_status);
	}
	/* second call to TRNG warm-up */
	_fips_status |= FIPS_POWER_UP_TEST_DONE;

	return get_time().val - starttime;
}

/**
 * Initialize FIPS mode. It is called on board init during power-up and resume
 */
static void fips_power_on(void)
{
	uint64_t testtime = -1ULL;
	/* make sure on power-on / resume it's cleared */
	_fips_status = FIPS_UNINITIALIZED;

	/**
	 * if this was a power-on or power-up tests weren't executed
	 * for some reason, run them
	 */
	if ((system_get_reset_flags() & EC_RESET_FLAG_POWER_ON) ||
	    !board_fips_power_up_done())
		testtime = fips_power_up_tests();
	else	/* tests were already completed before sleep */
		_fips_status |= FIPS_POWER_UP_TEST_DONE;

	/**
	 * once FIPS power-up tests completed we can enable console output
	 * and allow AP to boot. Both were disabled in init_board_properties()
	 */
	/* deassert_sys_rst(); */
		/* check if FIPS mode is  */
	if (fips_u2f_compliant())
		fips_set_status(FIPS_MODE_ACTIVE);

	console_enable_output();

	if (testtime != -1ULL)
		ccprints("FIPS power-up tests completed in %llu", testtime);

	if (_fips_status & FIPS_ERROR_MASK)
		ccprints("FIPS error 0x%08x", _fips_status);
	else
		ccprints("Running in FIPS 140-2 approved mode");
}

/**
 * FIPS settings depend on HW configuration in board_init, so
 * set priority lower than board_init which is HOOK_PRIO_DEFAULT
 */
DECLARE_HOOK(HOOK_INIT, fips_power_on, HOOK_PRIO_LAST);

static void fips_set_policy(bool active)
{
	/* do nothing if no change */
	if (_fips_status & FIPS_MODE_ACTIVE)
		return;
	/* TODO: we probably don't need this */
	board_set_local_fips_policy(active);
	ccprintf("FIPS policy set to %d\n", active);
	cflush();
	if (active) {
		/* wipe all CSPs */
		u2f_zeroize();
	} else {
		u2f_zeroize();
		/* TODO: create fake u2f keys old style */
		setvar(&k_salt, sizeof(k_salt), drbg_entropy0, 32);
	}
	/* nvmem_wipe_cache(); */
	/* TODO (sukhomlinov) consider EC_RESET_FLAG_SECURITY */
	system_reset(EC_RESET_FLAG_HARD);
}

/* TODO: add vendor command? */

static int cmd_fips_status(int argc, char **argv)
{
	ccprintf("FIPS status\ncrypto_allowed: %d\n",
		 fips_crypto_allowed());

	if (_fips_status == FIPS_UNINITIALIZED)
		ccprintf("FIPS mode not initialized\n");
	else if (_fips_status & FIPS_ERROR_MASK)
		ccprintf("FIPS error 0x%08x\n", _fips_status);
	else if ((_fips_status & FIPS_MODE_ACTIVE) &&
		 (_fips_status & FIPS_POWER_UP_TEST_DONE))
		ccprintf("FIPS-approved mode active 0x%08x\n", _fips_status);
	else
		ccprintf("FIPS not approved, status 0x%08x\n", _fips_status);

	cflush();

	if (argc == 2) {
		if (!strncmp(argv[1], "on", 2))
			fips_set_policy(true);
		else if (!strncmp(argv[1], "off", 3))
			fips_set_policy(false);
		else if (!strncmp(argv[1], "kat", 3))
			fips_power_up_tests();
		else if (!strncmp(argv[1], "trng", 4))
			fips_break_cmd = FIPS_BREAK_TRNG;
		else if (!strncmp(argv[1], "sha", 3))
			fips_break_cmd = FIPS_BREAK_SHA256;

	}
	return 0;
}

DECLARE_SAFE_CONSOLE_COMMAND(fips, cmd_fips_status, NULL, NULL);


/*
 * This extension command is similar to TPM2_GetRandom, but made
 * available for CRYPTO_TEST = 1 which disables TPM.
 * Command structure, shared out of band with the test driver running
 * on the host:
 *
 * field     |    size  |                  note
 * =========================================================================
 * op        |    1     | 0 - get status, 1 - set FIPS ON (remove old U2F)
 *           |          | 2 - run KAT, 3 - set TRNG error, 4 - set SHA error
 */
static enum vendor_cmd_rc fips_cmd(enum vendor_cmd_cc code, void *buf,
				    size_t input_size, size_t *response_size)
{
	uint8_t *cmd = buf;
	uint32_t fips_reverse;

	*response_size = 0;
	if (input_size != 1)
		return VENDOR_RC_BOGUS_ARGS;

	switch (*cmd) {
	case 0:
		fips_reverse = htobe32(_fips_status);
		memcpy(buf, &fips_reverse, sizeof(fips_reverse));
		*response_size = sizeof(fips_reverse);
		break;
	case 1:
		fips_set_policy(true); /* we can reboot here... */
		break;
	case 2:
		fips_power_up_tests();
		fips_reverse = htobe32(_fips_status);
		memcpy(buf, &fips_reverse, sizeof(fips_reverse));
		*response_size = sizeof(fips_reverse);
		break;
	case 3:
		fips_break_cmd = FIPS_BREAK_TRNG;
		break;
	case 4:
		fips_break_cmd = FIPS_BREAK_SHA256;
		break;
	case 5:
		fips_break_cmd = FIPS_BREAK_HMAC_SHA256;
		break;
	case 6:
		fips_break_cmd = FIPS_BREAK_HMAC_DRBG;
		break;
	case 7:
		fips_break_cmd = FIPS_BREAK_ECDSA;
		break;
	case 8:
		fips_break_cmd = FIPS_BREAK_AES256;
		break;
	case 9:
		fips_break_cmd = FIPS_NO_BREAK;
		break;
	default:
		return VENDOR_RC_BOGUS_ARGS;
	}

	return VENDOR_RC_SUCCESS;
}

DECLARE_VENDOR_COMMAND(VENDOR_CC_FIPS_CMD, fips_cmd);
