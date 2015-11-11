/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include "registers.h"
#include "util.h"

static void sha256_transform(SHA256_CTX *ctx);
static void sha256_init(SHA256_CTX *ctx);
static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, uint32_t len);
static const uint8_t *sha256_final(SHA256_CTX *ctx);
static const uint8_t *sha256_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest);
static const uint8_t *dcrypto_sha256_final(SHA256_CTX *ctx);

/* Software SHA256 implementation. */
static const uint32_t _SHA256_K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static const HASH_VTAB SW_SHA256_VTAB = {
	DCRYPTO_SHA256_init,
	sha256_update,
	sha256_final,
	sha256_hash,
	SHA256_DIGEST_BYTES
};

static void sha256_transform(SHA256_CTX *ctx)
{
#define _ROR(value, bits) (((value) >> (bits)) | ((value) << (32 - (bits))))
#define _SHR(value, bits) ((value) >> (bits))

	uint32_t W[64];
	uint32_t A, B, C, D, E, F, G, H;
	const uint8_t *p = ctx->buf;
	int t;

	for (t = 0; t < 16; ++t) {
		uint32_t tmp =  *p++ << 24;

		tmp |= *p++ << 16;
		tmp |= *p++ << 8;
		tmp |= *p++;
		W[t] = tmp;
	}

	for (; t < 64; t++) {
		uint32_t s0 = _ROR(W[t-15], 7) ^ _ROR(W[t-15], 18) ^
			_SHR(W[t-15], 3);
		uint32_t s1 = _ROR(W[t-2], 17) ^ _ROR(W[t-2], 19) ^
			_SHR(W[t-2], 10);

		W[t] = W[t-16] + s0 + W[t-7] + s1;
	}

	A = ctx->state[0];
	B = ctx->state[1];
	C = ctx->state[2];
	D = ctx->state[3];
	E = ctx->state[4];
	F = ctx->state[5];
	G = ctx->state[6];
	H = ctx->state[7];

	for (t = 0; t < 64; t++) {
		uint32_t s0 = _ROR(A, 2) ^ _ROR(A, 13) ^ _ROR(A, 22);
		uint32_t maj = (A & B) ^ (A & C) ^ (B & C);
		uint32_t t2 = s0 + maj;
		uint32_t s1 = _ROR(E, 6) ^ _ROR(E, 11) ^ _ROR(E, 25);
		uint32_t ch = (E & F) ^ ((~E) & G);
		uint32_t t1 = H + s1 + ch + _SHA256_K[t] + W[t];

		H = G;
		G = F;
		F = E;
		E = D + t1;
		D = C;
		C = B;
		B = A;
		A = t1 + t2;
	}

	ctx->state[0] += A;
	ctx->state[1] += B;
	ctx->state[2] += C;
	ctx->state[3] += D;
	ctx->state[4] += E;
	ctx->state[5] += F;
	ctx->state[6] += G;
	ctx->state[7] += H;

#undef _SHR
#undef _ROR
}

static void sha256_init(SHA256_CTX *ctx)
{
	ctx->vtab = &SW_SHA256_VTAB;
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
	ctx->count = 0;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, uint32_t len)
{
	int i = (int) (ctx->count & 63);
	const uint8_t *p = (const uint8_t *)data;

	ctx->count += len;
	while (len--) {
		ctx->buf[i++] = *p++;
		if (i == 64) {
			sha256_transform(ctx);
			i = 0;
		}
	}
}

static const uint8_t *sha256_final(SHA256_CTX *ctx)
{
	uint32_t cnt = ctx->count * 8;
	uint8_t tmp = 0x80;
	int i;

	sha256_update(ctx, &tmp, 1);

	tmp = 0;
	while ((ctx->count & 63) != 56)
		sha256_update(ctx, &tmp, 1);

	for (i = 0; i < 4; ++i)
		sha256_update(ctx, &tmp, 1);

	for (i = 0; i < 4; ++i) {
		tmp = (uint8_t) (cnt >> ((3 - i) * 8));
		sha256_update(ctx, &tmp, 1);
	}

	for (i = 0; i < SHA256_DIGEST_WORDS; i++)
		ctx->state[i] = __builtin_bswap32(ctx->state[i]);

	return (uint8_t *)ctx->state;
}

static const uint8_t *sha256_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest)
{
	int i;
	SHA256_CTX ctx;

	sha256_init(&ctx);
	sha256_update(&ctx, data, len);
	sha256_final(&ctx);

	for (i = 0; i < SHA256_DIGEST_WORDS; ++i)
		digest[i] = ctx.state[i];

	return (uint8_t *) digest;
}


/* Hardware SHA implementation. */
static const HASH_VTAB HW_SHA256_VTAB = {
	DCRYPTO_SHA256_init,
	dcrypto_sha_update,
	dcrypto_sha256_final,
	DCRYPTO_SHA256_hash,
	SHA256_DIGEST_BYTES
};

void DCRYPTO_SHA256_init(SHA256_CTX *ctx, uint32_t sw_required)
{
	if (!sw_required && hw_available) {
		ctx->vtab = &HW_SHA256_VTAB;
		dcrypto_sha_init(SHA1_MODE);
		hw_available = 0;
	} else {
		sha256_init(ctx);
	}
}

static const uint8_t *dcrypto_sha256_final(SHA256_CTX *ctx)
{
	dcrypto_sha_wait(SHA256_MODE, (uint32_t *) ctx->buf);
	hw_available = 1;
	return ctx->buf;
}

const uint8_t *DCRYPTO_SHA256_hash(const uint8_t *data, uint32_t n,
				uint8_t *digest)
{
	if (hw_available)
		dcrypto_sha_hash(SHA256_MODE, data, n, digest);
	else
		sha256_hash(data, n, digest);
	return digest;
}
