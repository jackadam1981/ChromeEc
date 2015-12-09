/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include <assert.h>


void bn_init(BIGNUM *b, uint8_t *buf, size_t len)
{
	/* Only word-multiple sized buffers accepted. */
	assert((len & 0x3) == 0);
	b->dmax = len / BN_BYTES;
	memset(buf, 0x00, len);
	b->d = (uint32_t *) buf;
}

int bn_check_topbit(const BIGNUM *N)
{
	return N->d[N->dmax - 1] & (1 << (32 - 1));
}

/* a[n]. */
static int bn_is_bit_set(const BIGNUM *a, int n)
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

/* a[] >= b[]. */
static int bn_gte(const BIGNUM *a, const BIGNUM *b)
{
	int i;

	for (i = a->dmax - 1; a->d[i] == b->d[i] && i > 0; --i)
		;
	return a->d[i] >= b->d[i];
}

/* c[] = c[] - a[], assumes c > a. */
static uint32_t bn_sub(BIGNUM *c, const BIGNUM *a)
{
	int64_t A = 0;
	int i;

	for (i = 0; i < a->dmax; i++) {
		A += (uint64_t) c->d[i] - a->d[i];
		c->d[i] = (uint32_t) A;
		A >>= 32;
	}
	return (uint32_t) A;  /* 0 or -1. */
}

/* c[] = c[] + a[]. */
static uint32_t bn_add(BIGNUM *c, const BIGNUM *a)
{
	uint64_t A = 0;
	int i;

	for (i = 0; i < a->dmax; ++i) {
		A += (uint64_t) c->d[i] + a->d[i];
		c->d[i] = (uint32_t) A;
		A >>= 32;
	}

	return (uint32_t) A;  /* 0 or 1. */
}

/* r[] <<= 1. */
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

/* Montgomery c[] += a * b[] / R % N. */
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
		A = tmp >> 32;  /* 0 or 1. */
		if (A)
			bn_sub(c, N);
	}
}

/* Montgomery c[] = a[] * b[] / R % N. */
static void bn_mont_mul(BIGNUM *c, const BIGNUM *a,
			const BIGNUM *b, const uint32_t nprime,
			const BIGNUM *N)
{
	int i;

	for (i = 0; i < N->dmax; i++)
		c->d[i] = 0;

	bn_mont_mul_add(c, a ? a->d[0] : 1, b, nprime, N);
	for (i = 1; i < N->dmax; i++)
		bn_mont_mul_add(c, a ? a->d[i] : 0, b, nprime, N);
}

/* Mongomery R * R % N, R = 1 << (1 + log2N). */
static void bn_compute_RR(BIGNUM *RR, const BIGNUM *N)
{
	int i;

	bn_sub(RR, N);         /* R - N = R % N since R < 2N */

	/* Repeat 2 * R % N, log2(R) times. */
	for (i = 0; i < N->dmax * BN_BITS2; i++) {
		if (bn_lshift(RR))
			assert(bn_sub(RR, N) == -1);
		if (bn_gte(RR, N))
			bn_sub(RR, N);
	}
}

/* Montgomery nprime = -1 / n0 % (2 ^ 32). */
static uint32_t bn_compute_nprime(const uint32_t n0)
{
	int i;
	uint32_t ninv = 1;

	/* Repeated Hensel lifting. */
	for (i = 0; i  < 5; i++)
		ninv *= 2 - (n0 * ninv);

	return ~ninv + 1;       /* Two's complement. */
}

/* Montgomery output = input ^ exp % N. */
void bn_mont_modexp(BIGNUM *output, const BIGNUM *input,
		const BIGNUM *exp, const BIGNUM *N)
{
	int i;
	uint32_t nprime;
	uint8_t RR_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t acc_buf[RSA_MAX_BYTES] __aligned(4);
	uint8_t aR_buf[RSA_MAX_BYTES] __aligned(4);

	BIGNUM RR;
	BIGNUM acc;
	BIGNUM aR;

	bn_init(&RR, RR_buf, bn_size(N));
	bn_init(&acc, acc_buf, bn_size(N));
	bn_init(&aR, aR_buf, bn_size(N));

	nprime = bn_compute_nprime(N->d[0]);
	bn_compute_RR(&RR, N);
	bn_mont_mul(&acc, NULL, &RR, nprime, N);      /* R = 1 * RR / R % N */
	bn_mont_mul(&aR, input, &RR, nprime, N);      /* aR = a * RR / R % N */
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
	if (output->d == (uint32_t *) &acc_buf[0]) {
		memcpy(&acc.d[0], acc_buf, bn_size(output));
		*output = acc;
		acc.d = (uint32_t *) &acc_buf[0];
	}

	if (bn_sub(output, N))
		bn_add(output, N);                      /* Final reduce. */
	output->dmax = N->dmax;

	memset(RR_buf, 0, sizeof(RR_buf));
	memset(acc_buf, 0, sizeof(acc_buf));
	memset(aR_buf, 0, sizeof(aR_buf));
}
