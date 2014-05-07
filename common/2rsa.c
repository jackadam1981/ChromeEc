/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implementation of RSA signature verification which uses a pre-processed
 * key for computation. The code extends Android's RSA verification code to
 * support multiple RSA key lengths and hash digest algorithms.
 */

#include "common.h"

struct vb2_public_key {
  uint32_t len;           /* Length of n[] and rr[] in number of uint32_t */
  uint32_t n0inv;         /* -1 / n[0] mod 2^32 */
  const uint32_t *n;      /* Modulus as little endian array */
  const uint32_t *rr;     /* R^2 as little endian array */
  uint32_t algorithm;     /* Algorithm to use when verifying with the key */
};

#if 1

/**
 * a[] -= mod
 */
static void subM(const struct vb2_public_key *key, uint32_t *a)
{
	int64_t A = 0;
	uint32_t i;
	for (i = 0; i < key->len; ++i) {
		A += (uint64_t)a[i] - key->n[i];
		a[i] = (uint32_t)A;
		A >>= 32;
	}
}

/**
 * Return a[] >= mod
 */
static int geM(const struct vb2_public_key *key, uint32_t *a)
{
	uint32_t i;
	for (i = key->len; i;) {
		--i;
		if (a[i] < key->n[i]) return 0;
		if (a[i] > key->n[i]) return 1;
	}
	return 1;  /* equal */
}

/**
 * Montgomery c[] += a * b[] / R % mod
 */
static void montMulAdd(const struct vb2_public_key *key,
                       uint32_t *c,
                       const uint32_t a,
                       const uint32_t *b)
{
	uint64_t A = (uint64_t)a * b[0] + c[0];
	uint32_t d0 = (uint32_t)A * key->n0inv;
	uint64_t B = (uint64_t)d0 * key->n[0] + (uint32_t)A;
	uint32_t i;

	for (i = 1; i < key->len; ++i) {
		A = (A >> 32) + (uint64_t)a * b[i] + c[i];
		B = (B >> 32) + (uint64_t)d0 * key->n[i] + (uint32_t)A;
		c[i - 1] = (uint32_t)B;
	}

	A = (A >> 32) + (B >> 32);

	c[i - 1] = (uint32_t)A;

	if (A >> 32) {
		subM(key, c);
	}
}

/**
 * Montgomery c[] = a[] * b[] / R % mod
 */
static void montMul(const struct vb2_public_key *key,
                    uint32_t *c,
                    const uint32_t *a,
                    const uint32_t *b)
{
	uint32_t i;
	for (i = 0; i < key->len; ++i) {
		c[i] = 0;
	}
	for (i = 0; i < key->len; ++i) {
		montMulAdd(key, c, a[i], b);
	}
}

/**
 * In-place public exponentiation. (65537}
 *
 * @param key		Key to use in signing
 * @param inout		Input and output big-endian byte array
 * @param workbuf32	Work buffer; caller must verify this is (3 * key->len)
 *			elements long.
 */
void modpowF4(const struct vb2_public_key *key, uint8_t *inout,
		    uint32_t *workbuf32)
{
	uint32_t *a = workbuf32;
	uint32_t *aR = a + key->len;
	uint32_t *aaR = aR + key->len;
	uint32_t *aaa = aaR;  /* Re-use location. */
	int i;

	/* Convert from big endian byte array to little endian word array. */
	for (i = 0; i < (int)key->len; ++i) {
		uint32_t tmp =
			(inout[((key->len - 1 - i) * 4) + 0] << 24) |
			(inout[((key->len - 1 - i) * 4) + 1] << 16) |
			(inout[((key->len - 1 - i) * 4) + 2] << 8) |
			(inout[((key->len - 1 - i) * 4) + 3] << 0);
		a[i] = tmp;
	}

	montMul(key, aR, a, key->rr);  /* aR = a * RR / R mod M   */
	for (i = 0; i < 16; i+=2) {
		montMul(key, aaR, aR, aR);  /* aaR = aR * aR / R mod M */
		montMul(key, aR, aaR, aaR);  /* aR = aaR * aaR / R mod M */
	}
	montMul(key, aaa, aR, a);  /* aaa = aR * a / R mod M */


	/* Make sure aaa < mod; aaa is at most 1x mod too large. */
	if (geM(key, aaa)) {
		subM(key, aaa);
	}

	/* Convert to bigendian byte array */
	for (i = (int)key->len - 1; i >= 0; --i) {
		uint32_t tmp = aaa[i];
		*inout++ = (uint8_t)(tmp >> 24);
		*inout++ = (uint8_t)(tmp >> 16);
		*inout++ = (uint8_t)(tmp >>  8);
		*inout++ = (uint8_t)(tmp >>  0);
	}
}

#endif
