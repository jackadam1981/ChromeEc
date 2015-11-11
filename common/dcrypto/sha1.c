/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"

#include "registers.h"

static void sha1_transform(SHA1_CTX *ctx);
static void sha1_init(SHA1_CTX *ctx);
static void sha1_update(SHA1_CTX *ctx, const uint8_t *data, uint32_t len);
static const uint8_t *sha1_final(SHA1_CTX *ctx);
static const uint8_t *sha1_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest);
static const uint8_t *dcrypto_sha1_final(SHA1_CTX *unused);

/* Software SHA1 implementation. */
static const HASH_VTAB SW_SHA1_VTAB = {
	DCRYPTO_SHA1_init,
	sha1_update,
	sha1_final,
	sha1_hash,
	SHA1_DIGEST_BYTES
};

#define ROL(bits, value) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(SHA1_CTX *ctx)
{
	uint32_t W[80];
	uint32_t A, B, C, D, E;
	uint8_t *p = ctx->buf;
	int t;

	for (t = 0; t < 16; ++t) {
		uint32_t tmp =  *p++ << 24;

		tmp |= *p++ << 16;
		tmp |= *p++ << 8;
		tmp |= *p++;
		W[t] = tmp;
	}

	for (; t < 80; t++)
		W[t] = ROL(1, W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);

	A = ctx->state[0];
	B = ctx->state[1];
	C = ctx->state[2];
	D = ctx->state[3];
	E = ctx->state[4];

	for (t = 0; t < 80; t++) {
		uint32_t tmp = ROL(5, A) + E + W[t];

		if (t < 20)
			tmp += (D^(B&(C^D))) + 0x5A827999;
		else if (t < 40)
			tmp += (B^C^D) + 0x6ED9EBA1;
		else if (t < 60)
			tmp += ((B&C)|(D&(B|C))) + 0x8F1BBCDC;
		else
			tmp += (B^C^D) + 0xCA62C1D6;

		E = D;
		D = C;
		C = ROL(30, B);
		B = A;
		A = tmp;
	}

	ctx->state[0] += A;
	ctx->state[1] += B;
	ctx->state[2] += C;
	ctx->state[3] += D;
	ctx->state[4] += E;
}

static void sha1_init(SHA1_CTX *ctx)
{
	ctx->vtab = &SW_SHA1_VTAB;
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
}

static void sha1_update(SHA1_CTX *ctx, const uint8_t *data, uint32_t len)
{
	uint32_t i = (uint32_t)(ctx->count & 63);
	const uint8_t *p = (const uint8_t *) data;

	ctx->count += len;
	while (len--) {
		ctx->buf[i++] = *p++;
		if (i == 64) {
			sha1_transform(ctx);
			i = 0;
		}
	}
}

static const uint8_t *sha1_final(SHA1_CTX *ctx)
{
	uint8_t *p = ctx->buf;
	uint64_t cnt = ctx->count * 8;
	int i;

	sha1_update(ctx, (uint8_t *)"\x80", 1);
	while ((ctx->count & 63) != 56)
		sha1_update(ctx, (uint8_t *)"\0", 1);

	for (i = 0; i < 8; ++i) {
		uint8_t tmp = (uint8_t) (cnt >> ((7 - i) * 8));

		sha1_update(ctx, &tmp, 1);
	}

	for (i = 0; i < 5; i++) {
		uint32_t tmp = ctx->state[i];

		*p++ = (uint8_t)(tmp >> 24);
		*p++ = (uint8_t)(tmp >> 16);
		*p++ = (uint8_t)(tmp >> 8);
		*p++ = (uint8_t)(tmp >> 0);
	}

	return ctx->buf;
}

static const uint8_t *sha1_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest)
{
	SHA1_CTX ctx;

	sha1_init(&ctx);
	sha1_update(&ctx, data, len);
	memcpy(digest, sha1_final(&ctx), SHA1_DIGEST_BYTES);
	return digest;
}


/*
 * Hardware SHA implementation.
 */
static const HASH_VTAB HW_SHA1_VTAB = {
	DCRYPTO_SHA1_init,
	dcrypto_sha_update,
	dcrypto_sha1_final,
	DCRYPTO_SHA1_hash,
	SHA1_DIGEST_BYTES
};

int hw_available = 1;

void dcrypto_sha_wait(enum sha_mode mode, uint32_t *digest)
{
	int i;
	const int digest_len = (mode == SHA1_MODE) ?
		SHA1_DIGEST_BYTES :
		SHA256_DIGEST_BYTES;

	/* Stop LIVESTREAM mode. */
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_STOP, 1);
	/* Wait for SHA DONE interrupt. */
	while (!GREG32(KEYMGR, SHA_ITOP))
		;

	/* Read out final digest. */
	for (i = 0; i < digest_len / 4; ++i)
		*digest++ = GR_KEYMGR_SHA_HASH(i);
}

void dcrypto_sha_hash(enum sha_mode mode, const uint8_t *data, uint32_t n,
		uint8_t *digest)
{
	dcrypto_sha_init(mode);
	dcrypto_sha_update(NULL, data, n);
	dcrypto_sha_wait(mode, (uint32_t *) digest);
}

void dcrypto_sha_init(enum sha_mode mode)
{
	/* Stop LIVESTREAM mode, in case final() was not called. */
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_STOP, 1);
	/* Clear interrupt status. */
	GREG32(KEYMGR, SHA_ITOP) = 0;
	/* SHA1 or SHA256 mode */
	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, SHA1, mode == SHA1_MODE ? 1 : 0);
	/* Enable streaming mode. */
	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, LIVESTREAM, 1);
	/* Enable the SHA DONE interrupt. */
	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, INT_EN_DONE, 1);
	/* Start SHA engine. */
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_GO, 1);
}

/* Select and initialize either the software or hardware
 * implementation.  If "multi-threaded" behaviour is required, then
 * callers must set sw_required to 1.  This is because SHA1 state
 * internal to the hardware cannot be extracted, so it is not possible
 * to suspend and resume a hardware based SHA operation.
 *
 * If the caller has no preference as to implementation, then hardware
 * is preferred based on availability.  Hardware is considered
 * occupied between init() and finished() calls. */
void DCRYPTO_SHA1_init(SHA1_CTX *ctx, uint32_t sw_required)
{
	if (!sw_required && hw_available) {
		ctx->vtab = &HW_SHA1_VTAB;
		dcrypto_sha_init(SHA1_MODE);
		hw_available = 0;
	} else {
		sha1_init(ctx);
	}
}

void dcrypto_sha_update(HASH_CTX *unused, const uint8_t *data, uint32_t n)
{
	const uint8_t *bp = (const uint8_t *) data;
	const uint32_t *wp;

	/* Feed unaligned start bytes. */
	while (n != 0 && ((uint32_t)bp & 3)) {
		GREG8(KEYMGR, SHA_INPUT_FIFO) = *bp++;
		n -= 1;
	}

	/* Feed groups of aligned words. */
	wp = (uint32_t *)bp;
	while (n >= 8*4) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		n -= 8*4;
	}
	/* Feed individual aligned words. */
	while (n >= 4) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *wp++;
		n -= 4;
	}

	/* Feed remaing bytes. */
	bp = (uint8_t *) wp;
	while (n != 0) {
		GREG8(KEYMGR, SHA_INPUT_FIFO) = *bp++;
		n -= 1;
	}
}

static const uint8_t *dcrypto_sha1_final(SHA1_CTX *ctx)
{
	dcrypto_sha_wait(SHA1_MODE, (uint32_t *) ctx->buf);
	hw_available = 1;
	return ctx->buf;
}

const uint8_t *DCRYPTO_SHA1_hash(const uint8_t *data, uint32_t n,
				uint8_t *digest)
{
	if (hw_available)
		dcrypto_sha_hash(SHA1_MODE, data, n, digest);
	else
		sha1_hash(data, n, digest);
	return digest;
}

