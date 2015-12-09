/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include <assert.h>

#define BN_BITS2        32
#define BN_BYTES        4

/* TODO: TRNG.  RAND sizeof SHA256 digest. */
static const uint8_t RAND[] = {
	0x9c, 0xb1, 0xd0, 0x8b, 0x88, 0x02, 0x9b, 0x07,
	0xd5, 0x83, 0xa8, 0xe2, 0x21, 0x2f, 0x3d, 0x76,
	0xa7, 0x34, 0x84, 0xbd, 0xc3, 0xd5, 0x4d, 0x7c,
	0x1f, 0x77, 0x4f, 0xb7, 0x8c, 0xa6, 0x09, 0x0c
};

static void bn_init(BIGNUM *bn, uint8_t *buf, size_t len)
{
	bn->dmax = len / BN_BYTES;
	memset(buf, 0x00, len);
	bn->d = (uint32_t *) buf;
}

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

	/* TODO: constant time. */
	for (i = PS - padded; i <  max_msg_len + 1; i++) {
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

#define RSA_PKCS1_PADDING_SIZE 11

static int pkcs1_pad(uint8_t *padded, uint32_t padded_len,
		const uint8_t *in, uint32_t in_len)
{
	if (padded_len < RSA_PKCS1_PADDING_SIZE)
		return 0;
	if (in_len > padded_len - RSA_PKCS1_PADDING_SIZE)
		return 0;

	*(padded++) = 0;
	*(padded++) = 1;
	memset(padded, 0xFF, padded_len - 3 - in_len);
	padded += padded_len - 3 - in_len;
	*(padded++) = 0;
	memcpy(padded, in, in_len);
	return 1;
}

static int check_pkcs1_pad(uint8_t *out, uint32_t *out_len,
			const uint8_t *padded, uint32_t padded_len)
{
	int i;

	if (padded_len < RSA_PKCS1_PADDING_SIZE)
		return 0;

	/* TODO: constant time. */
	if (padded[0] != 0 || padded[1] != 1)
		return 0;
	for (i = 2; i < padded_len; i++) {
		if (padded[i] == 0xFF)
			continue;
		else if (padded[i] == 0)
			break;
		return 0;
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

static int check_topbit(const BIGNUM *N)
{
	return N->d[N->dmax - 1] & (1 << (BN_BITS2 - 1));
}

/* Return -1/n0 % (2^32) */
static uint32_t compute_nprime(const uint32_t n0)
{
	int i;
	uint32_t ninv = 1;

	/* Repeated Hensel lifting. */
	for (i = 0; i  < 5; i++)
		ninv *= 2 - (n0 * ninv);

	return ~ninv + 1;       /* Two's complement. */
}

/* Return true if a >= b */
static int bn_gte(const BIGNUM *a, const BIGNUM *b)
{
	int i;

	for (i = a->dmax - 1; a->d[i] == b->d[i] && i > 0; --i)
		;
	return a->d[i] >= b->d[i];
}

int bn_is_bit_set(const BIGNUM *a, int n)
{
	int i, j;

	if (n < 0)
		return 0;

	i = n / BN_BITS2;
	j = n % BN_BITS2;
	if (a->dmax <= i)
		return 0;


	return (a->d[i] >> j) & 1;
}

/* Compute r = r - a, assumes r > a. */
static uint32_t bn_sub(BIGNUM *r, const BIGNUM *a)
{
	int64_t A = 0;
	int i;

	for (i = 0; i < r->dmax; i++) {
		A += (uint64_t) r->d[i] - a->d[i];
		r->d[i] = (uint32_t) A;
		A >>= 32;
	}
	return (uint32_t) A;  /* 0 or -1 */
}

static uint32_t bn_lshift(BIGNUM *r)
{
	int i;
	uint32_t w;
	uint32_t carry = 0;

	for (i = 0; i < r->dmax; i++) {
		w = (r->d[i] << 1) | carry;
		carry = (r->d[i] & 0x80000000) >> 31;
		r->d[i] = w;
	}
	return carry;
}

static uint32_t bn_add(BIGNUM *c, const BIGNUM *N)
{
	uint64_t A = 0;
	int i;

	for (i = 0; i < N->dmax; ++i) {
		A += (uint64_t) c->d[i] + N->d[i];
		c->d[i] = (uint32_t) A;
		A >>= 32;
	}

	return (uint32_t) A;  /* 0 or 1 */
}

/* Montgomery c[] += a * b[] / R % N */
static void bn_mont_mul_add(BIGNUM *c, const uint32_t a, const BIGNUM *b,
		const uint32_t nprime, const BIGNUM *N)
{
	uint32_t A, B, d0;
	int i;

	{
		register uint64_t tmp;

		tmp = c->d[0] + (uint64_t) a * b->d[0];
		A = tmp >> 32;
		d0 = (uint32_t) tmp * (uint32_t) nprime;
		tmp = (uint32_t)tmp + (uint64_t) d0 * N->d[0];
		B = tmp >> 32;
	}

	for (i = 0; i < N->dmax - 1;) {
		register uint64_t tmp;

		tmp = A + (uint64_t) a * b->d[i + 1] + c->d[i + 1];
		A = tmp >> 32;
		tmp = B + (uint64_t) d0 * N->d[i + 1] + (uint32_t) tmp;
		c->d[i] = (uint32_t) tmp;
		B = tmp >> 32;
		++i;
	}

	{
		uint64_t tmp = (uint64_t) A + B;

		c->d[i] = (uint32_t) tmp;
		A = tmp >> 32;  /* 0 or 1 */
		if (A)
			bn_sub(c, N);
	}
}

/* Montgomery c[] = a[] * b[] / R % N. */
static void bn_mont_mul(BIGNUM *c, const BIGNUM *a,
			const BIGNUM *b, const uint32_t nprime,
			const BIGNUM *N)
{
	size_t i;

	for (i = 0; i < N->dmax; i++)
		c->d[i] = 0;

	bn_mont_mul_add(c, a ? a->d[0] : 1, b, nprime, N);
	for (i = 1; i < N->dmax; i++)
		bn_mont_mul_add(c, a ? a->d[i] : 0, b, nprime, N);
}

static void compute_RR(BIGNUM *RR, const BIGNUM *N)
{
	int i;

	/* R - N = R % N since R < 2N */
	bn_sub(RR, N);

	/* Repeat 2*R % N, log2(R) times. */
	for (i = 0; i < N->dmax * BN_BITS2; i++) {
		if (bn_lshift(RR))
			assert(bn_sub(RR, N) == -1);
		if (bn_gte(RR, N))
			bn_sub(RR, N);
	}
}

static void bn_mont_modexp(BIGNUM *output, const BIGNUM *input,
			const BIGNUM *exp, uint32_t nprime, const BIGNUM *RR,
			const BIGNUM *N)
{
	int i;
	uint8_t acc_buf[RSA_MAX_BYTES];
	uint8_t aR_buf[RSA_MAX_BYTES];
	BIGNUM acc;
	BIGNUM aR;

	bn_init(&acc, acc_buf, N->dmax * BN_BYTES);
	bn_init(&aR, aR_buf, N->dmax * BN_BYTES);

	bn_mont_mul(&acc, NULL, RR, nprime, N);      /* R = 1 * RR / R % N */
	bn_mont_mul(&aR, input, RR, nprime, N);      /* aR = a * RR / R % N */
	output->d[0] = 1;

	/* TODO: burn stack space and use windowing. */
	for (i = exp->dmax * BN_BITS2 - 1; i >= 0; i--) {
		bn_mont_mul(output, &acc, &acc, nprime, N);
		if (bn_is_bit_set(exp, i)) {
			bn_mont_mul(&acc, output, &aR, nprime, N);
		} else {
			BIGNUM tmp = *output;

			*output = acc;
			acc = tmp;
		}
	}

	bn_mont_mul(output, NULL, &acc, nprime, N);     /* Convert out. */
	if (bn_sub(output, N))
		bn_add(output, N);                      /* Final reduce. */
	output->dmax = N->dmax;
}

int DCRYPTO_rsa_encrypt(RSA *rsa, uint8_t *out, uint32_t *out_len,
			const uint8_t *in, const uint32_t in_len,
			enum padding_mode padding, enum hashing_mode hashing,
			const char *label)
{
	uint32_t nprime;
	uint8_t RR_buf[RSA_MAX_BYTES];
	uint8_t padded_buf[RSA_MAX_BYTES];
	uint8_t e_buf[BN_BYTES];

	BIGNUM RR;
	BIGNUM padded;
	BIGNUM e;
	BIGNUM encrypted;

	if (!check_topbit(&rsa->N))            /* Check that top bit is set. */
		return 0;
	if (rsa->N.dmax * BN_BYTES > RSA_MAX_BYTES)
		return 0;                      /* Unsupported key size. */
	if (*out_len < rsa->N.dmax * BN_BYTES)
		return 0;                      /* Output buffer too small. */

	bn_init(&padded, padded_buf, rsa->N.dmax * BN_BYTES);
	bn_init(&RR, RR_buf, rsa->N.dmax * BN_BYTES);
	bn_init(&encrypted, out, rsa->N.dmax * BN_BYTES);
	bn_init(&e, e_buf, sizeof(e_buf));
	*e.d = rsa->e;

	switch (padding) {
	case PADDING_MODE_OAEP:
		if (!oaep_pad((uint8_t *) padded.d, padded.dmax * BN_BYTES,
				(const uint8_t *) in, in_len, hashing, label))
			return 0;
		break;
	case PADDING_MODE_PKCS1:
		if (!pkcs1_pad((uint8_t *) padded.d, padded.dmax * BN_BYTES,
				(const uint8_t *) in, in_len))
			return 0;
		break;
	default:
		return 0;                       /* Unsupported padding mode. */
	}

	nprime = compute_nprime(rsa->N.d[0]);
	compute_RR(&RR, &rsa->N);
	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) padded.d, padded.dmax * BN_BYTES);
	bn_mont_modexp(&encrypted, &padded, &e, nprime, &RR, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) encrypted.d, encrypted.dmax * BN_BYTES);
	*out_len = encrypted.dmax * BN_BYTES;

	memset(RR_buf, 0, sizeof(RR_buf));
	memset(padded_buf, 0, sizeof(padded_buf));
	memset(e_buf, 0, sizeof(e_buf));
	return 1;
}

int DCRYPTO_rsa_decrypt(RSA *rsa, uint8_t *out, uint32_t *out_len,
			const uint8_t *in, const uint32_t in_len,
			enum padding_mode padding, enum hashing_mode hashing,
			const char *label)
{
	uint32_t nprime;
	uint8_t RR_buf[RSA_MAX_BYTES];
	uint8_t encrypted_buf[RSA_MAX_BYTES];
	uint8_t padded_buf[RSA_MAX_BYTES];

	BIGNUM RR;
	BIGNUM encrypted;
	BIGNUM padded;
	int ret = 1;

	if (rsa->N.dmax * BN_BYTES > RSA_MAX_BYTES)
		return 0;                      /* Unsupported key size. */
	if (in_len != rsa->N.dmax * BN_BYTES)
		return 0;                      /* Invalid input length. */
	if (*out_len < rsa->N.dmax * BN_BYTES)
		return 0;                      /* Output buffer too small. */
	if (!check_topbit(&rsa->N))            /* Check that top bit is set. */
		return 0;

	/* TODO: eliminate this copy. */
	memcpy(encrypted_buf, in, in_len);
	bn_init(&encrypted, encrypted_buf, in_len);
	bn_init(&padded, padded_buf, in_len);
	bn_init(&RR, RR_buf, rsa->N.dmax * BN_BYTES);

	nprime = compute_nprime(rsa->N.d[0]);
	compute_RR(&RR, &rsa->N);
	/* Reverse from big-endien to little-endien notation. */
	reverse((uint8_t *) encrypted.d, encrypted.dmax * BN_BYTES);
	bn_mont_modexp(&padded, &encrypted, &rsa->d, nprime, &RR, &rsa->N);
	/* Back to big-endien notation. */
	reverse((uint8_t *) padded.d, padded.dmax * BN_BYTES);

	switch (padding) {
	case PADDING_MODE_OAEP:
		if (!check_oaep_pad(out, out_len, (uint8_t *) padded.d,
					padded.dmax * BN_BYTES, hashing, label))
			ret = 0;
		break;
	case PADDING_MODE_PKCS1:
		if (!check_pkcs1_pad(out, out_len, (const uint8_t *) padded.d,
					padded.dmax * BN_BYTES))
			ret = 0;
		break;
	default:
		/* Unsupported padding mode. */
		ret = 0;
		break;
	}

	memset(RR_buf, 0, sizeof(RR_buf));
	memset(encrypted_buf, 0, sizeof(encrypted_buf));
	memset(padded_buf, 0, sizeof(padded_buf));
	return ret;
}
