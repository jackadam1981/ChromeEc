/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cryptoc/util.h"
#include "dcrypto.h"
#include "internal.h"

/* V = HMAC_K(V) */
static void update_v(const uint32_t *k, uint32_t *v)
{
	LITE_HMAC_CTX ctx;

	DCRYPTO_HMAC_SHA256_init(&ctx, k, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, v, SHA256_DIGEST_SIZE);
	memcpy(v, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);
}

/* K = HMAC_K(V || tag || x || h1) */
static void update_k(uint32_t *k, const uint32_t *v, uint8_t tag,
		     const uint32_t *x,  const uint32_t *h1)
{
	LITE_HMAC_CTX ctx;

	DCRYPTO_HMAC_SHA256_init(&ctx, k, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, v, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, &tag, 1);
	HASH_update(&ctx.hash, x, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, h1, SHA256_DIGEST_SIZE);
	memcpy(k, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);
}

/* K = HMAC_K(V || 0x00) */
static void append_0(uint32_t *k, const uint32_t *v)
{
	LITE_HMAC_CTX ctx;
	uint8_t zero = 0;

	DCRYPTO_HMAC_SHA256_init(&ctx, k, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, v, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, &zero, 1);
	memcpy(k, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);
}

/* Deterministic generation of k as per RFC 6979 */
static void init_det_k(const uint32_t *x, const uint32_t *h1, uint32_t *k,
		       uint32_t *v)
{
	/* V = 0x01 0x01 0x01 ... 0x01 */
	always_memset(v,  0x01, SHA256_DIGEST_SIZE);
	/* K = 0x00 0x00 0x00 ... 0x00 */
	always_memset(k,  0x00, SHA256_DIGEST_SIZE);
	/* K = HMAC_K(V || 0x00 || int2octets(x) || bits2octets(h1)) */
	update_k(k, v, 0x00, x, h1);
	/* V = HMAC_K(V) */
	update_v(k, v);
	/* K = HMAC_K(V || 0x01 || int2octets(x) || bits2octets(h1)) */
	update_k(k, v, 0x01, x, h1);
	/* V = HMAC_K(V) */
	update_v(k, v);
}

/* Current state of K & V, they are 'protected' by the dcrypto lock. */
static uint32_t stateful_k[SHA256_DIGEST_WORDS];
static uint32_t stateful_v[SHA256_DIGEST_WORDS];

static void stir_det_k(uint32_t *k_out)
{
	/* V = HMAC_K(V) */
	update_v(stateful_k, stateful_v);
	/* get the current candidate K, then prepare for the next one */
	memcpy(k_out, stateful_v, SHA256_DIGEST_SIZE);
	/* K = HMAC_K(V || 0x00) */
	append_0(stateful_k, stateful_v);
	/* V = HMAC_K(V) */
	update_v(stateful_k, stateful_v);
}

int dcrypto_p256_ecdsa_sign_det(const p256_int *key, const p256_int *message,
		p256_int *r, p256_int *s)
{
	int result;

	dcrypto_init_and_lock();
	init_det_k(key->a, message->a, stateful_k, stateful_v);
	result = dcrypto_p256_ecdsa_internal(key, message, r, s, stir_det_k);
	dcrypto_unlock();

	return result;
}
