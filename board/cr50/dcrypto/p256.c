/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"

/* P-256 elliptic curve characteristics */
const p256_int SECP256r1_nMin1 = {
	{
		0xfc632551 - 1,
		0xf3b9cac2,
		0xa7179e84,
		0xbce6faad,
		-1,
		-1,
		0,
		-1,
	},
};

const p256_int SECP256r1_nMin2 = /* P-256 curve order - 2 */
	{ { 0xfc632551 - 2, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, -1, -1, 0,
	    -1 } };

/**
 * The 32 bit LFSR whose maximum length feedback polynomial is represented
 * as X^32 + X^22 + X^2 + X^1 + 1 will produce 2^32-1 PN sequence.
 * Polynomial is bit reversed as we shift left, not right.
 * This LFSR can be initialized with 0,  but can't be initialized with
 * 0xFFFFFFFF. This is handy to avoid non-zero initial values.
 */
static uint32_t next_fast_random(uint32_t seed)
{
	/**
	 * 12                        22          32
	 * 1100 0000 0000 0000 0000 0100 0000 0001 = 0xC0000401
	 */
	uint32_t mask = (-((int32_t)seed >= 0)) & 0xC0000401;

	return (seed << 1) ^ mask;
}
static uint32_t fast_random_state;

void set_fast_random_seed(uint32_t seed)
{
	/* Avoid prohibited value as LFSR seed value. */
	if (next_fast_random(seed) == seed)
		seed++;
	fast_random_state = seed;
}

uint32_t fast_random(void)
{
	fast_random_state = next_fast_random(fast_random_state);
	return fast_random_state;
}

void p256_clear(p256_int *a)
{
	always_memset(a, 0, sizeof(*a));
}

int p256_is_zero(const p256_int *a)
{
	int result = 0;

	for (size_t i = 0; i < P256_NDIGITS; ++i)
		result |= P256_DIGIT(a, i);
	return !result;
}

/* b = a + d. Returns carry, 0 or 1. */
int p256_add_d(const p256_int *a, uint32_t d, p256_int *b)
{
	int i;
	p256_ddigit carry = d;

	for (i = 0; i < P256_NDIGITS; ++i) {
		carry += (p256_ddigit)P256_DIGIT(a, i);
		if (b)
			P256_DIGIT(b, i) = (p256_digit)carry;
		carry >>= P256_BITSPERDIGIT;
	}
	return (int)carry;
}

/* Return -1 if a < b, and a != 0. */
static int p256_not_zero_and_lt_blinded(const p256_int *a, const p256_int *b)
{
	p256_sddigit borrow = 0;
	p256_digit zero = 0;

	for (int i = 0; i < P256_NDIGITS; ++i) {
		volatile uint32_t blinder = fast_random();

		zero |= P256_DIGIT(a, i);
		borrow += ((p256_sddigit)P256_DIGIT(a, i) - blinder);
		borrow -= P256_DIGIT(b, i);
		borrow += blinder;
		borrow >>= P256_BITSPERDIGIT;
	}
	return (int)borrow + (zero == 0);
}

/* Return -1, 0, 1 for a < b, a == b or a > b respectively. */
int p256_cmp(const p256_int *a, const p256_int *b)
{
	int i;
	p256_sddigit borrow = 0;
	p256_digit notzero = 0;

	for (i = 0; i < P256_NDIGITS; ++i) {
		borrow += (p256_sddigit)P256_DIGIT(a, i) - P256_DIGIT(b, i);
		/**
		 * Track whether any result digit is ever not zero.
		 * Relies on !!(non-zero) evaluating to 1, e.g., !!(-1)
		 * evaluating to 1.
		 */
		notzero |= !!((p256_digit)borrow);
		borrow >>= P256_BITSPERDIGIT;
	}
	return (int)borrow | notzero;
}

void p256_to_bin(const p256_int *src, uint8_t dst[P256_NBYTES])
{
	int i;
	uint8_t *p = &dst[0];

	for (i = P256_NDIGITS - 1; i >= 0; --i) {
		p256_digit digit = P256_DIGIT(src, i);

		p[0] = (uint8_t)(digit >> 24);
		p[1] = (uint8_t)(digit >> 16);
		p[2] = (uint8_t)(digit >> 8);
		p[3] = (uint8_t)(digit);
		p += 4;
	}
}

void p256_from_bin(const uint8_t src[P256_NBYTES], p256_int *dst)
{
	int i;
	const uint8_t *p = &src[0];

	for (i = P256_NDIGITS - 1; i >= 0; --i) {
		P256_DIGIT(dst, i) = (p[0] << 24) | (p[1] << 16) | (p[2] << 8) |
				     p[3];
		p += 4;
	}
}

int p256_is_odd(const p256_int *a)
{
	return P256_DIGIT(a, 0) & 1;
}

void p256_fast_random(p256_int *rnd)
{
	for (size_t i = 0; i < P256_NDIGITS; i++)
		P256_DIGIT(rnd, i) = fast_random();
}

enum hmac_result p256_hmac_drbg_generate(struct drbg_ctx *ctx, p256_int *rnd)
{
	enum hmac_result result;

	/* fill destination with something in case DRBG fails. */
	p256_fast_random(rnd);

	/* Generate p256 candidates from DRBG until valid is found. */
	do {
		result = hmac_drbg_generate(ctx, rnd->a, sizeof(rnd->a), NULL,
					    0);
	} while ((result == HMAC_DRBG_SUCCESS) &&
		 (p256_not_zero_and_lt_blinded(rnd, &SECP256r1_nMin1) >= 0));
	return result;
}
