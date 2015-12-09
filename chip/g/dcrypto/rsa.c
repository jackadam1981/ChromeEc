/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include <assert.h>

/* TODO: TRNG.  Fixed seed for OAEP pad. */
static const uint8_t RAND[] = {
	0x9c, 0xb1, 0xd0, 0x8b, 0x88, 0x02, 0x9b, 0x07,
	0xd5, 0x83, 0xa8, 0xe2, 0x21, 0x2f, 0x3d, 0x76,
	0xa7, 0x34, 0x84, 0xbd, 0xc3, 0xd5, 0x4d, 0x7c,
	0x1f, 0x77, 0x4f, 0xb7, 0x8c, 0xa6, 0x09, 0x0c
};

#if 0
#define CPRINTF(format, args...) cprintf(CC_EXTENSION, format, ## args)

static void array_dump(const uint8_t *p, size_t len, const char *name)
{
	int i;

	CPRINTF("%s:\n", name);
	for (i = 0; i < len; i++) {
		CPRINTF("%2X:", p[i]);
		if ((i + 1) % 16 == 0)
			CPRINTF("\n", p[i]);
	}
	CPRINTF("\n");
}

static void bn_dump(const BIGNUM *b, const char *name)
{
	const uint8_t *p = (uint8_t *) b->d;

	array_dump(p, bn_size(b), name);
}
#endif

static void MGF1_xor(uint8_t *dst, uint32_t dst_len,
		const uint8_t *seed, uint32_t seed_len,
		enum hashing_mode hashing)
{
	struct HASH_CTX ctx;
	struct {
		uint8_t b3;
		uint8_t b2;
		uint8_t b1;
		uint8_t b0;
	} cnt;
	const uint8_t *digest;
	const size_t hash_size = (hashing == HASH_SHA1) ? SHA1_DIGEST_BYTES
		: SHA256_DIGEST_BYTES;

	cnt.b0 = cnt.b1 = cnt.b2 = cnt.b3 = 0;
	while (dst_len) {
		int i;

		if (hashing == HASH_SHA1)
			DCRYPTO_SHA1_init(&ctx, 0);
		else
			DCRYPTO_SHA256_init(&ctx, 0);

		DCRYPTO_HASH_update(&ctx, seed, seed_len);
		DCRYPTO_HASH_update(&ctx, (uint8_t *) &cnt, sizeof(cnt));
		digest = DCRYPTO_HASH_final(&ctx);
		for (i = 0; i < dst_len && i < hash_size; ++i)
			*dst++ ^= *digest++;
		dst_len -= i;
		if (!++cnt.b0)
			++cnt.b1;
	}
}

/*
 * struct OAEP {                  // MSB to LSB.
 *      uint8_t zero;
 *      uint8_t seed[HASH_SIZE];
 *      uint8_t phash[HASH_SIZE];
 *      uint8_t PS[];             // Variable length (optional) zero-pad.
 *      uint8_t one;              // 0x01, message demarcator.
 *      uint8_t msg[];            // Input message.
 * };
 */
/* encrypt */
static int oaep_pad(uint8_t *output, uint32_t output_len,
		const uint8_t *msg, uint32_t msg_len,
		enum hashing_mode hashing, const char *label)
{
	const size_t hash_size = (hashing == HASH_SHA1) ? SHA1_DIGEST_BYTES
		: SHA256_DIGEST_BYTES;
	uint8_t *const seed = output + 1;
	uint8_t *const phash = seed + hash_size;
	uint8_t *const PS = phash + hash_size;
	const uint32_t max_msg_len = output_len - 2 - 2 * hash_size;
	const uint32_t ps_len = max_msg_len - msg_len;
	uint8_t *const one = PS + ps_len;
	struct HASH_CTX ctx;

	if (output_len < 2 + 2 * hash_size)
		return 0;       /* Key size too small for chosen hash. */
	if (msg_len > output_len - 2 - 2 * hash_size)
		return 0;       /* Input message too large for key size. */

	memset(output, 0, output_len);
	memcpy(seed, RAND, hash_size);
	if (hashing == HASH_SHA1)
		DCRYPTO_SHA1_init(&ctx, 0);
	else
		DCRYPTO_SHA256_init(&ctx, 0);

	DCRYPTO_HASH_update(&ctx, label, label ? strlen(label) : 0);
	memcpy(phash, DCRYPTO_HASH_final(&ctx), hash_size);
	*one = 1;
	memcpy(one + 1, msg, msg_len);
	MGF1_xor(phash, hash_size + 1 + max_msg_len,
		seed, hash_size, hashing);
	MGF1_xor(seed, hash_size, phash, hash_size + 1 + max_msg_len,
		hashing);
	return 1;
}

/* decrypt */
/* TODO: constant time. */
static int check_oaep_pad(uint8_t *out, uint32_t *out_len,
			uint8_t *padded, uint32_t padded_len,
			enum hashing_mode hashing, const char *label)
{
	const size_t hash_size = (hashing == HASH_SHA1) ? SHA1_DIGEST_BYTES
		: SHA256_DIGEST_BYTES;
	uint8_t *seed = padded + 1;
	uint8_t *phash = seed + hash_size;
	uint8_t *PS = phash + hash_size;
	const uint32_t max_msg_len = padded_len - 2 - 2 * hash_size;
	struct HASH_CTX ctx;
	int one_index = -1;
	int bad;
	int i;

	if (padded_len < 2 + 2 * hash_size)
		return 0;       /* Invalid input size. */

	/* Recover seed. */
	MGF1_xor(seed, hash_size, phash, hash_size + 1 + max_msg_len, hashing);
	/* Recover db. */
	MGF1_xor(phash, hash_size + 1 + max_msg_len, seed, hash_size, hashing);

	if (hashing == HASH_SHA1)
		DCRYPTO_SHA1_init(&ctx, 0);
	else
		DCRYPTO_SHA256_init(&ctx, 0);
	DCRYPTO_HASH_update(&ctx, label, label ? strlen(label) : 0);

	bad = memcmp(phash, DCRYPTO_HASH_final(&ctx), hash_size);
	bad |= padded[0];

	for (i = PS - padded; i <  padded_len; i++) {
		if (padded[i] == 1) {
			one_index = i;
			break;
		} else if (padded[i] != 0) {
			bad = 1;
			break;
		}
	}

	if (one_index < 0 || bad)
		return 0;
	one_index++;
	if (*out_len < padded_len - one_index)
		return 0;
	memcpy(out, padded + one_index, padded_len - one_index);
	*out_len = padded_len - one_index;
	return 1;
}

/* Constants from RFC 3447. */
#define RSA_PKCS1_PADDING_SIZE 11

/* encrypt */
static int pkcs1_type2_pad(uint8_t *padded, uint32_t padded_len,
		const uint8_t *in, uint32_t in_len)
{
	if (padded_len < RSA_PKCS1_PADDING_SIZE)
		return 0;
	if (in_len > padded_len - RSA_PKCS1_PADDING_SIZE)
		return 0;

	*(padded++) = 0;
	*(padded++) = 2;
	/* TODO: TRNG */
	memset(padded, 0x27, padded_len - 3 - in_len);
	padded += padded_len - 3 - in_len;
	*(padded++) = 0;
	memcpy(padded, in, in_len);
	return 1;
}

/* decrypt */
/* TODO: constant time */
static int check_pkcs1_type2_pad(uint8_t *out, uint32_t *out_len,
				const uint8_t *padded, uint32_t padded_len)
{
	int i;

	if (padded_len < RSA_PKCS1_PADDING_SIZE)
		return 0;
	if (padded[0] != 0 || padded[1] != 2)
		return 0;
	for (i = 2; i < padded_len; i++) {
		if (padded[i] == 0)
			break;
	}

	if (i == padded_len)
		return 0;
	i++;
	if (i < RSA_PKCS1_PADDING_SIZE)
		return 0;
	if (*out_len < padded_len - i)
		return 0;
	memcpy(out, &padded[i], padded_len - i);
	*out_len = padded_len - i;
	return 1;
}

static const uint8_t SHA1_DER[] = {
	0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e,
	0x03, 0x02, 0x1a, 0x05, 0x00, 0x04, 0x14
};
static const uint8_t SHA256_DER[] = {
	0x30, 0x31, 0x30, 0x0D, 0x06, 0x09, 0x60, 0x86,
	0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
	0x00, 0x04, 0x20
};

/* sign */
static int pkcs1_type1_pad(uint8_t *padded, uint32_t padded_len,
			const uint8_t *in, uint32_t in_len,
			enum hashing_mode hashing)
{
	const uint8_t *der = (hashing == HASH_SHA1) ? &SHA1_DER[0]
		: &SHA256_DER[0];
	const uint32_t der_size = (hashing == HASH_SHA1) ? sizeof(SHA1_DER)
		: sizeof(SHA256_DER);
	const uint32_t hash_size = (hashing == HASH_SHA1) ? SHA1_DIGEST_BYTES
		: SHA256_DIGEST_BYTES;
	uint32_t ps_len;

	if (padded_len < RSA_PKCS1_PADDING_SIZE + der_size)
		return 0;
	if (in_len != hash_size)
		return 0;
	if (in_len > padded_len - RSA_PKCS1_PADDING_SIZE - der_size)
		return 0;
	ps_len = padded_len - 3 - der_size - in_len;

	*(padded++) = 0;
	*(padded++) = 1;
	memset(padded, 0xFF, ps_len);
	padded += ps_len;
	*(padded++) = 0;
	memcpy(padded, der, der_size);
	padded += der_size;
	memcpy(padded, in, in_len);
	return 1;
}

/* verify */
/* TODO: constant time */
static int check_pkcs1_type1_pad(const uint8_t *msg, uint32_t msg_len,
				const uint8_t *padded, uint32_t padded_len,
				enum hashing_mode hashing)
{
	int i;
	const uint8_t *der = (hashing == HASH_SHA1) ? &SHA1_DER[0]
		: &SHA256_DER[0];
	const uint32_t der_size = (hashing == HASH_SHA1) ? sizeof(SHA1_DER)
		: sizeof(SHA256_DER);
	const uint32_t hash_size = (hashing == HASH_SHA1) ? SHA1_DIGEST_BYTES
		: SHA256_DIGEST_BYTES;
	uint32_t ps_len;

	if (msg_len != hash_size)
		return 0;
	if (padded_len < RSA_PKCS1_PADDING_SIZE + der_size + hash_size)
		return 0;
	ps_len = padded_len - 3 - der_size - hash_size;

	if (padded[0] != 0 || padded[1] != 1)
		return 0;
	for (i = 2; i < ps_len + 2; i++) {
		if (padded[i] != 0xFF)
			return 0;
	}

	if (padded[i++] != 0)
		return 0;
	if (memcmp(&padded[i], der, der_size) != 0)
		return 0;
	i += der_size;
	return memcmp(msg, &padded[i], hash_size) == 0;
}

static void reverse(uint8_t *start, size_t len)
{
	int i;
	uint8_t *end = start + len;

	for (i = 0; i < len / 2; ++i) {
		uint8_t tmp = *start;

		*start++ = *--end;
		*end = tmp;
	}
}

static int check_modulus_params(const BIGNUM *N, uint32_t *out_len)
{
	if (bn_size(N) > RSA_MAX_BYTES)
		return 0;                      /* Unsupported key size. */
	if (!bn_check_topbit(N))               /* Check that top bit is set. */
		return 0;
	if (out_len && *out_len < bn_size(N))
		return 0;                      /* Output buffer too small. */
	return 1;
}

int DCRYPTO_rsa_encrypt(RSA *rsa, uint8_t *out, uint32_t *out_len,
			const uint8_t *in, const uint32_t in_len,
			enum padding_mode padding, enum hashing_mode hashing,
			const char *label)
{
	uint8_t padded_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t e_buf[BN_BYTES] __aligned(4);

	BIGNUM padded;
	BIGNUM e;
	BIGNUM encrypted;

	if (!check_modulus_params(&rsa->N, out_len))
		return 0;

	bn_init(&padded, padded_buf, bn_size(&rsa->N));
	bn_init(&encrypted, out, bn_size(&rsa->N));
	bn_init(&e, e_buf, sizeof(e_buf));
	*e.d = rsa->e;

	switch (padding) {
	case PADDING_MODE_OAEP:
		if (!oaep_pad((uint8_t *) padded.d, bn_size(&padded),
				(const uint8_t *) in, in_len, hashing, label))
			return 0;
		break;
	case PADDING_MODE_PKCS1:
		if (!pkcs1_type2_pad((uint8_t *) padded.d, bn_size(&padded),
					(const uint8_t *) in, in_len))
			return 0;
		break;
	default:
		return 0;                       /* Unsupported padding mode. */
	}

	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) padded.d, bn_size(&padded));
	bn_mont_modexp(&encrypted, &padded, &e, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) encrypted.d, bn_size(&encrypted));
	*out_len = bn_size(&encrypted);

	memset(padded_buf, 0, sizeof(padded_buf));
	memset(e_buf, 0, sizeof(e_buf));
	return 1;
}

int DCRYPTO_rsa_decrypt(RSA *rsa, uint8_t *out, uint32_t *out_len,
			const uint8_t *in, const uint32_t in_len,
			enum padding_mode padding, enum hashing_mode hashing,
			const char *label)
{
	uint8_t encrypted_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t padded_buf[RSA_MAX_BYTES] __aligned(4);

	BIGNUM encrypted;
	BIGNUM padded;
	int ret = 1;

	if (!check_modulus_params(&rsa->N, out_len))
		return 0;
	if (in_len != bn_size(&rsa->N))
		return 0;                      /* Invalid input length. */

	/* TODO: this copy can be eliminated if input may be modified. */
	bn_init(&encrypted, encrypted_buf, in_len);
	memcpy(encrypted_buf, in, in_len);
	bn_init(&padded, padded_buf, in_len);

	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) encrypted.d, encrypted.dmax * BN_BYTES);
	bn_mont_modexp(&padded, &encrypted, &rsa->d, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) padded.d, padded.dmax * BN_BYTES);

	switch (padding) {
	case PADDING_MODE_OAEP:
		if (!check_oaep_pad(out, out_len, (uint8_t *) padded.d,
					bn_size(&padded), hashing, label))
			ret = 0;
		break;
	case PADDING_MODE_PKCS1:
		if (!check_pkcs1_type2_pad(
				out, out_len, (const uint8_t *) padded.d,
				bn_size(&padded)))
			ret = 0;
		break;
	default:
		/* Unsupported padding mode. */
		ret = 0;
		break;
	}

	memset(encrypted_buf, 0, sizeof(encrypted_buf));
	memset(padded_buf, 0, sizeof(padded_buf));
	return ret;
}

int DCRYPTO_rsa_sign(RSA *rsa, uint8_t *out, uint32_t *out_len,
		const uint8_t *in, const uint32_t in_len,
		enum padding_mode padding, enum hashing_mode hashing)
{
	uint8_t padded_buf[RSA_MAX_BYTES] __aligned(4);

	BIGNUM padded;
	BIGNUM signature;

	if (!check_modulus_params(&rsa->N, out_len))
		return 0;

	bn_init(&padded, padded_buf, bn_size(&rsa->N));
	bn_init(&signature, out, bn_size(&rsa->N));

	/* TODO: add support for PSS. */
	switch (padding) {
	case PADDING_MODE_PKCS1:
		if (!pkcs1_type1_pad((uint8_t *) padded.d, bn_size(&padded),
					(const uint8_t *) in, in_len, hashing))
			return 0;
		break;
	default:
		return 0;
	}

	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) padded.d, bn_size(&padded));
	bn_mont_modexp(&signature, &padded, &rsa->d, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) signature.d, bn_size(&signature));
	*out_len = bn_size(&rsa->N);

	memset(padded_buf, 0, sizeof(padded_buf));
	return 1;
}

int DCRYPTO_rsa_verify(RSA *rsa, const uint8_t *digest, uint32_t digest_len,
		const uint8_t *sig, const uint32_t sig_len,
		enum padding_mode padding, enum hashing_mode hashing)
{
	uint8_t padded_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t signature_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t e_buf[BN_BYTES] __aligned(4);

	BIGNUM padded;
	BIGNUM signature;
	BIGNUM e;
	int ret = 1;

	if (!check_modulus_params(&rsa->N, NULL))
		return 0;
	if (sig_len != bn_size(&rsa->N))
		return 0;                      /* Invalid input length. */

	bn_init(&signature, signature_buf, bn_size(&rsa->N));
	memcpy(signature_buf, sig, bn_size(&rsa->N));
	bn_init(&padded, padded_buf, bn_size(&rsa->N));
	bn_init(&e, e_buf, sizeof(e_buf));
	*e.d = rsa->e;

	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) signature.d, signature.dmax * BN_BYTES);
	bn_mont_modexp(&padded, &signature, &e, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) padded.d, padded.dmax * BN_BYTES);

	switch (padding) {
	case PADDING_MODE_PKCS1:
		if (!check_pkcs1_type1_pad(
				digest, digest_len, (uint8_t *) padded.d,
				bn_size(&padded), hashing))
			ret = 0;
		break;
	default:
		/* Unsupported padding mode. */
		ret = 0;
		break;
	}

	memset(padded_buf, 0, sizeof(padded_buf));
	memset(signature_buf, 0, sizeof(signature_buf));
	return ret;
}


#if 0
/* Code for running on locally. */
const uint8_t RSA_768_N[96] = {
	0x69, 0x85, 0x39, 0x2d, 0x78, 0x2b, 0x90, 0x75,
	0xe1, 0x7c, 0xc1, 0x7b, 0xbd, 0x5b, 0xdd, 0xfd,
	0x00, 0x36, 0xf7, 0x38, 0x74, 0x33, 0x2b, 0xa8,
	0x53, 0x89, 0x10, 0xa7, 0x2d, 0x3c, 0xe6, 0x00,
	0xa3, 0xe5, 0x8b, 0x5f, 0xed, 0x77, 0x32, 0xc0,
	0x0f, 0xe2, 0x2c, 0x51, 0x1b, 0x46, 0xba, 0x18,
	0xc0, 0x4e, 0x1b, 0x44, 0xdf, 0x94, 0xcc, 0x15,
	0xe1, 0x67, 0x48, 0x3a, 0x12, 0xc4, 0x0c, 0x82,
	0xd2, 0xfa, 0xfe, 0x74, 0x6e, 0x49, 0xa4, 0x8b,
	0x64, 0xc2, 0x3b, 0x33, 0x36, 0x72, 0x24, 0xdb,
	0x17, 0x86, 0x5a, 0x35, 0xd2, 0x23, 0x20, 0xd4,
	0x7c, 0xf0, 0x32, 0xd9, 0x46, 0xed, 0xdb, 0xb0
};

const uint8_t RSA_768_D[96] = {
	0x01, 0x40, 0x76, 0x7b, 0x41, 0xd6, 0xd9, 0x17,
	0xfe, 0x52, 0x6d, 0xdd, 0x24, 0x70, 0xbc, 0x97,
	0x7e, 0xcf, 0x54, 0x22, 0x4c, 0x71, 0x29, 0xf5,
	0xb2, 0xe2, 0xf6, 0xf8, 0x8b, 0x9e, 0x20, 0x1a,
	0x1e, 0x67, 0xee, 0x59, 0xf9, 0x83, 0x6b, 0x91,
	0x8d, 0xdf, 0x03, 0xfc, 0xdd, 0x0f, 0x35, 0xd7,
	0xa2, 0x5d, 0x06, 0x3f, 0x45, 0xb9, 0xb0, 0x23,
	0x90, 0x7b, 0x11, 0x32, 0xc1, 0xf2, 0x12, 0xdb,
	0x61, 0xf9, 0xa7, 0x31, 0x24, 0xc8, 0x66, 0x4e,
	0x49, 0x72, 0xb9, 0xce, 0xa6, 0x5b, 0xab, 0x46,
	0x45, 0xdf, 0x75, 0x76, 0x3e, 0xd3, 0x42, 0x9f,
	0x5c, 0x1b, 0x8c, 0x25, 0x50, 0xb9, 0xad, 0xae

};

const uint8_t RSA_2048_N[256] = {
	0x99, 0xa9, 0x93, 0xdf, 0xe8, 0xde, 0x41, 0x07,
	0xe9, 0xb1, 0x4f, 0x53, 0xa6, 0x11, 0xe3, 0x67,
	0x88, 0xc5, 0x9a, 0x57, 0xa5, 0x38, 0x1f, 0x69,
	0x51, 0xf2, 0xa7, 0xb5, 0x6a, 0xd2, 0x1a, 0xf2,
	0x0c, 0x62, 0xad, 0x33, 0x1f, 0x82, 0x21, 0x4a,
	0x72, 0xb3, 0x6e, 0xba, 0xfd, 0x66, 0x3e, 0xef,
	0x40, 0x78, 0xa7, 0x37, 0x97, 0x4a, 0x74, 0x63,
	0x23, 0x05, 0x2e, 0x55, 0x6d, 0x36, 0xd0, 0xb7,
	0x8c, 0xb7, 0x83, 0x60, 0x3b, 0xa1, 0x58, 0x5d,
	0xdc, 0xef, 0xf7, 0x2c, 0x5e, 0x05, 0x27, 0xbc,
	0xb0, 0x4d, 0xc9, 0xff, 0x04, 0x50, 0x22, 0x97,
	0xe7, 0x15, 0x66, 0xa5, 0x24, 0x0e, 0x86, 0xa6,
	0x36, 0x9c, 0x92, 0xa2, 0x16, 0x51, 0xed, 0xc2,
	0xea, 0xbf, 0xf4, 0xb2, 0x5e, 0x3a, 0xd7, 0xc5,
	0xa3, 0xfa, 0xf0, 0xcf, 0xcf, 0x7b, 0xc8, 0x5c,
	0x07, 0xe2, 0xcc, 0xa8, 0xb8, 0x36, 0x76, 0xb1,
	0xc9, 0xbb, 0x48, 0x38, 0xbe, 0x0b, 0x57, 0xce,
	0x05, 0x2d, 0xf1, 0xdb, 0x7b, 0x94, 0xb6, 0xcd,
	0x3a, 0xa8, 0x50, 0x49, 0xca, 0x18, 0xb3, 0x52,
	0x18, 0x49, 0xde, 0x10, 0xf8, 0x41, 0x40, 0x6e,
	0x51, 0xaf, 0xdd, 0x06, 0xc3, 0x30, 0xc7, 0x57,
	0x6b, 0xd4, 0xdc, 0x10, 0x46, 0x30, 0x04, 0x23,
	0x98, 0xc0, 0xf0, 0xb4, 0xeb, 0x5d, 0xc9, 0x6e,
	0x50, 0x1f, 0xd7, 0xd9, 0xac, 0xf2, 0x0d, 0x06,
	0xe3, 0x9b, 0x5e, 0xde, 0x2a, 0xaa, 0xb1, 0xaf,
	0xd6, 0x97, 0x68, 0x2d, 0xeb, 0x0c, 0x7b, 0x75,
	0x49, 0x23, 0x64, 0xbe, 0x90, 0x53, 0x82, 0x99,
	0xa2, 0x50, 0x78, 0x0c, 0x9f, 0x72, 0xc1, 0x0a,
	0x0f, 0x32, 0x75, 0xed, 0x1f, 0x6e, 0xef, 0x2c,
	0x2e, 0x1d, 0x4c, 0x19, 0x85, 0x5c, 0x90, 0x95,
	0xe3, 0x4b, 0x86, 0xf5, 0xb7, 0x9f, 0x73, 0xcd,
	0xbe, 0x15, 0x8e, 0x43, 0x2e, 0x61, 0xd7, 0x9c
};

const uint8_t RSA_2048_D[256] = {
	0xf5, 0x95, 0x99, 0xca, 0x31, 0x84, 0x66, 0x4c,
	0xa9, 0x29, 0x24, 0x74, 0x22, 0x29, 0xb4, 0x64,
	0x5b, 0x22, 0xeb, 0x5d, 0x2f, 0xe3, 0x62, 0x21,
	0x02, 0x16, 0x33, 0x16, 0xe4, 0xad, 0x10, 0x52,
	0x3f, 0xf0, 0xf1, 0x86, 0x68, 0x54, 0x47, 0x24,
	0xcc, 0x5c, 0x08, 0x82, 0x0f, 0x68, 0xdd, 0x79,
	0x55, 0x11, 0x07, 0x6d, 0x56, 0x89, 0x30, 0xf1,
	0x7f, 0xaf, 0xb1, 0xb8, 0x41, 0xe8, 0x7a, 0x82,
	0x03, 0x1a, 0x95, 0xd7, 0x00, 0x7c, 0xb7, 0x04,
	0xee, 0x8e, 0x9b, 0xbc, 0x4f, 0xdf, 0xa8, 0x38,
	0xea, 0xbf, 0xfb, 0x79, 0xa0, 0xd3, 0xd6, 0xc2,
	0x1f, 0x67, 0xa2, 0x88, 0x2b, 0x1d, 0x23, 0xc6,
	0x19, 0xfc, 0x27, 0x45, 0xcf, 0xbd, 0xc7, 0xe9,
	0x6e, 0x7a, 0xe2, 0x84, 0x4c, 0x9c, 0x16, 0x65,
	0xb0, 0xa6, 0x88, 0xc5, 0xbe, 0x30, 0x70, 0xb9,
	0xc6, 0x6d, 0x3f, 0xf5, 0xcd, 0x52, 0x97, 0x54,
	0x15, 0x26, 0xd2, 0x06, 0x82, 0xcc, 0xe7, 0x02,
	0x1a, 0x23, 0xb8, 0x0a, 0x71, 0xde, 0x91, 0x82,
	0xe4, 0x1e, 0xbe, 0x67, 0xeb, 0x94, 0x24, 0x22,
	0xe7, 0x27, 0xfa, 0x52, 0xf2, 0x94, 0x5e, 0x6e,
	0x85, 0xc1, 0x47, 0x42, 0xdc, 0xae, 0x8b, 0xaf,
	0x4e, 0x32, 0xc6, 0x8d, 0xd3, 0xc0, 0xa2, 0x6b,
	0x02, 0x96, 0x76, 0x0a, 0x96, 0x87, 0x16, 0x35,
	0xc1, 0xea, 0xf7, 0x91, 0xa4, 0xa3, 0x1b, 0x40,
	0xc0, 0x95, 0x20, 0x14, 0x9f, 0x32, 0xad, 0x39,
	0x19, 0x29, 0xea, 0x80, 0x33, 0x2c, 0x31, 0x86,
	0xca, 0x5e, 0x89, 0xf0, 0x74, 0xdf, 0x8f, 0xdc,
	0xa3, 0xf3, 0xbe, 0x26, 0xd0, 0xa3, 0xb4, 0x7c,
	0x6e, 0xdf, 0xad, 0xdb, 0x26, 0xf3, 0xaa, 0xfb,
	0x68, 0x56, 0x43, 0xb9, 0x7f, 0x19, 0x70, 0x67,
	0x5a, 0x66, 0x15, 0x6f, 0xe2, 0x14, 0x8f, 0xbc,
	0x89, 0x8b, 0x4a, 0xdf, 0x1f, 0x02, 0x9d, 0x4e
};

int main(void)
{
	RSA rsa;
	uint8_t out[256];
	uint32_t out_len = sizeof(out);
	const char *in = "Hello CR50!";
	uint8_t decrypted[256];
	uint32_t decrypted_len = sizeof(decrypted);
	FILE *f;

	rsa.e = 65537;
	rsa.N.dmax = 64;
	rsa.N.d = (uint32_t *) RSA_2048_N;
	rsa.d.dmax = 64;
	rsa.d.d = (uint32_t *) RSA_2048_D;
	int i;

	if (!DCRYPTO_rsa_encrypt(&rsa, out, &out_len, in, strlen(in),
					PADDING_MODE_PKCS1, HASH_SHA1, "")) {
		fprintf(stderr, "encrypt FAILED\n");
		return 1;
	}

	f = fopen("/tmp/enc", "w");
	if (fwrite(out, 1, out_len, f) != out_len) {
		fprintf(stderr, "fwrite failed\n");
		return 1;
	}
	fclose(f);
	f = NULL;

	if (!DCRYPTO_rsa_decrypt(&rsa, decrypted, &decrypted_len, out, out_len,
					PADDING_MODE_PKCS1, HASH_SHA1, "")) {
		fprintf(stderr, "decrypt FAILED\n");
		return 1;
	}

	fprintf(stderr, "SUCCESS\n");
	return 0;
}
#endif
