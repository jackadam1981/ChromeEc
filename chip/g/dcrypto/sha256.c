/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "dcrypto.h"
#include "internal.h"
#include "registers.h"
#include "util.h"

static const uint8_t *dcrypto_sha256_final(SHA256_CTX *ctx);

#ifdef SECTION_IS_RO
/* RO is single threaded. */
#define mutex_lock(x)
#define mutex_unlock(x)
static inline int dcrypto_grab_sha_hw(void)
{
	return 1;
}
static inline void dcrypto_release_sha_hw(void)
{
}
#else
#include "task.h"
static struct mutex hw_busy_mutex;

static void sha256_init(SHA256_CTX *ctx);
static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, uint32_t len);
static const uint8_t *sha256_final(SHA256_CTX *ctx);
static const uint8_t *sha256_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest);

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

static const struct HASH_VTAB SW_SHA256_VTAB = {
	sha256_update,
	sha256_final,
	sha256_hash,
	SHA256_DIGEST_BYTES
};

static void sha256_init(SHA256_CTX *ctx)
{
	ctx->vtab = &SW_SHA256_VTAB;
	SHA256_init(&ctx->u.sw_sha256);
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, uint32_t len)
{
	SHA256_update(&ctx->u.sw_sha256, data, len);
}

static const uint8_t *sha256_final(SHA256_CTX *ctx)
{
	return SHA256_final(&ctx->u.sw_sha256);
}

static const uint8_t *sha256_hash(const uint8_t *data, uint32_t len,
				uint8_t *digest)
{
	SHA256_CTX ctx;

	sha256_init(&ctx);
	sha256_update(&ctx, data, len);
	sha256_final(&ctx);

	memcpy(digest, ctx.u.sw_sha256.buf, SHA256_DIGEST_WORDS);

	return digest;
}

static int hw_busy;

int dcrypto_grab_sha_hw(void)
{
	int rv = 0;

	mutex_lock(&hw_busy_mutex);
	if (!hw_busy) {
		rv = 1;
		hw_busy = 1;
	}
	mutex_unlock(&hw_busy_mutex);

	return rv;
}

void dcrypto_release_sha_hw(void)
{
	mutex_lock(&hw_busy_mutex);
	hw_busy = 0;
	mutex_unlock(&hw_busy_mutex);
}

#endif  /* ! SECTION_IS_RO */

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
	dcrypto_release_sha_hw();
}

/* Hardware SHA implementation. */
static const struct HASH_VTAB HW_SHA256_VTAB = {
	dcrypto_sha_update,
	dcrypto_sha256_final,
	DCRYPTO_SHA256_hash,
	SHA256_DIGEST_BYTES
};

void dcrypto_sha_hash(enum sha_mode mode, const uint8_t *data, uint32_t n,
		uint8_t *digest)
{
	dcrypto_sha_init(mode);
	dcrypto_sha_update(NULL, data, n);
	dcrypto_sha_wait(mode, (uint32_t *) digest);
}

void dcrypto_sha_update(struct HASH_CTX *unused,
			const uint8_t *data, uint32_t n)
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

void DCRYPTO_SHA256_init(SHA256_CTX *ctx, uint32_t sw_required)
{
	if (!sw_required && dcrypto_grab_sha_hw()) {
		ctx->vtab = &HW_SHA256_VTAB;
		dcrypto_sha_init(SHA1_MODE);
	}
#ifndef SECTION_IS_RO
	else
		sha256_init(ctx);
#endif
}

static const uint8_t *dcrypto_sha256_final(SHA256_CTX *ctx)
{
	dcrypto_sha_wait(SHA256_MODE, (uint32_t *) ctx->u.hardware.buf);
	dcrypto_release_sha_hw();
	return ctx->u.hardware.buf;
}

const uint8_t *DCRYPTO_SHA256_hash(const uint8_t *data, uint32_t n,
				uint8_t *digest)
{
	if (dcrypto_grab_sha_hw())
		/* dcrypto_sha_wait() will release the hw. */
		dcrypto_sha_hash(SHA256_MODE, data, n, digest);
#ifndef SECTION_IS_RO
	else
		sha256_hash(data, n, digest);
#endif
	return digest;
}
