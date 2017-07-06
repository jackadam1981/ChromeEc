/* SHA-256 and SHA-512 implementation based on code by Oliver Gay
 * <olivier.gay@a3.epfl.ch> under a BSD-style license. See below.
 */

/*
 * FIPS 180-2 SHA-224/256/384/512 implementation
 * Last update: 02/02/2007
 * Issue date:  04/30/2005
 *
 * Copyright (C) 2005, 2007 Olivier Gay <olivier.gay@a3.epfl.ch>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "console.h"
#include "sha256.h"
#include "util.h"

#ifdef CONFIG_SHA256_HW
#include "sha256_chip.h"
#endif

/* Debug macros for console test command */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SWITCH, format, ## args)


#define SHFR(x, n)    (x >> n)
#define ROTR(x, n)   ((x >> n) | (x << ((sizeof(x) << 3) - n)))
#define ROTL(x, n)   ((x << n) | (x >> ((sizeof(x) << 3) - n)))
#define CH(x, y, z)  ((x & y) ^ (~x & z))
#define MAJ(x, y, z) ((x & y) ^ (x & z) ^ (y & z))

#define SHA256_F1(x) (ROTR(x,  2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SHA256_F2(x) (ROTR(x,  6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SHA256_F3(x) (ROTR(x,  7) ^ ROTR(x, 18) ^ SHFR(x,  3))
#define SHA256_F4(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHFR(x, 10))

#define UNPACK32(x, str)				\
	{						\
		*((str) + 3) = (uint8_t) ((x));		\
		*((str) + 2) = (uint8_t) ((x) >>  8);	\
		*((str) + 1) = (uint8_t) ((x) >> 16);	\
		*((str) + 0) = (uint8_t) ((x) >> 24);	\
	}

#define PACK32(str, x)						\
	{							\
		*(x) = ((uint32_t) *((str) + 3))		\
			| ((uint32_t) *((str) + 2) <<  8)	\
			| ((uint32_t) *((str) + 1) << 16)	\
			| ((uint32_t) *((str) + 0) << 24);	\
	}

/* Macros used for loops unrolling */

#ifndef CONFIG_SHA256_HW

#define SHA256_SCR(i)						\
	{							\
		w[i] =  SHA256_F4(w[i -  2]) + w[i -  7]	\
			+ SHA256_F3(w[i - 15]) + w[i - 16];	\
	}

#define SHA256_EXP(a, b, c, d, e, f, g, h, j)				\
	{								\
		t1 = wv[h] + SHA256_F2(wv[e]) + CH(wv[e], wv[f], wv[g])	\
			+ sha256_k[j] + w[j];				\
		t2 = SHA256_F1(wv[a]) + MAJ(wv[a], wv[b], wv[c]);	\
		wv[d] += t1;						\
		wv[h] = t1 + t2;					\
	}

static const uint32_t sha256_h0[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

static const uint32_t sha256_k[64] = {
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
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#endif /* #ifndef CONFIG_SHA256_HW */

void SHA256_init(struct sha256_ctx *ctx)
{
#ifdef CONFIG_SHA256_HW
	chip_sha256_init(ctx);
#else
	int i;

	for (i = 0; i < 8; i++)
		ctx->h[i] = sha256_h0[i];

	ctx->len = 0;
	ctx->tot_len = 0;
#endif /* #ifdef CONFIG_SHA256_HW */
}

#ifndef CONFIG_SHA256_HW
static void SHA256_transform(struct sha256_ctx *ctx, const uint8_t *message,
			     unsigned int block_nb)
{
	/* Note: this function requires a considerable amount of stack */
	uint32_t w[64];
	uint32_t wv[8];
	uint32_t t1, t2;
	const unsigned char *sub_block;
	int i, j;

	for (i = 0; i < (int) block_nb; i++) {
		sub_block = message + (i << 6);

		for (j = 0; j < 16; j++)
			PACK32(&sub_block[j << 2], &w[j]);

		for (j = 16; j < 64; j++)
			SHA256_SCR(j);

		for (j = 0; j < 8; j++)
			wv[j] = ctx->h[j];

		for (j = 0; j < 64; j++) {
			t1 = wv[7] + SHA256_F2(wv[4]) + CH(wv[4], wv[5], wv[6])
				+ sha256_k[j] + w[j];
			t2 = SHA256_F1(wv[0]) + MAJ(wv[0], wv[1], wv[2]);
			wv[7] = wv[6];
			wv[6] = wv[5];
			wv[5] = wv[4];
			wv[4] = wv[3] + t1;
			wv[3] = wv[2];
			wv[2] = wv[1];
			wv[1] = wv[0];
			wv[0] = t1 + t2;
		}

		for (j = 0; j < 8; j++)
			ctx->h[j] += wv[j];
	}
}
#endif /* #ifndef CONFIG_SHA256_HW */

void SHA256_update(struct sha256_ctx *ctx, const uint8_t *data, uint32_t len)
{
#ifdef CONFIG_SHA256_HW
	chip_sha256_update(ctx, data, len);
#else
	unsigned int block_nb;
	unsigned int new_len, rem_len, tmp_len;
	const uint8_t *shifted_data;

	tmp_len = SHA256_BLOCK_SIZE - ctx->len;
	rem_len = len < tmp_len ? len : tmp_len;

	memcpy(&ctx->block[ctx->len], data, rem_len);

	if (ctx->len + len < SHA256_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;
	block_nb = new_len / SHA256_BLOCK_SIZE;

	shifted_data = data + rem_len;

	SHA256_transform(ctx, ctx->block, 1);
	SHA256_transform(ctx, shifted_data, block_nb);

	rem_len = new_len % SHA256_BLOCK_SIZE;

	memcpy(ctx->block, &shifted_data[block_nb << 6], rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 6;
#endif
}

/*
 * Specialized SHA256_init + SHA256_update that takes the first data block of
 * size SHA256_BLOCK_SIZE as input.
 */
static void SHA256_init_1b(struct sha256_ctx *ctx, const uint32_t *data)
{
#ifdef CONFIG_SHA256_HW
	chip_sha256_init_1b(ctx, data);
#else
	int i;

	for (i = 0; i < 8; i++)
		ctx->h[i] = sha256_h0[i];

	SHA256_transform(ctx, (uint8_t *)data, 1);

	ctx->len = 0;
	ctx->tot_len = SHA256_BLOCK_SIZE;
#endif
}

uint8_t *SHA256_final(struct sha256_ctx *ctx)
{
#ifdef CONFIG_SHA256_HW
	return chip_sha256_final(ctx);
#else
	unsigned int block_nb;
	unsigned int pm_len;
	unsigned int len_b;
	int i;

	block_nb = (1 + ((SHA256_BLOCK_SIZE - 9)
			 < (ctx->len % SHA256_BLOCK_SIZE)));

	len_b = (ctx->tot_len + ctx->len) << 3;
	pm_len = block_nb << 6;

	memset(ctx->block + ctx->len, 0, pm_len - ctx->len);
	ctx->block[ctx->len] = 0x80;
	UNPACK32(len_b, ctx->block + pm_len - 4);

	SHA256_transform(ctx, ctx->block, block_nb);

	for (i = 0; i < 8; i++)
		UNPACK32(ctx->h[i], &ctx->buf[i << 2]);

	return ctx->buf;
#endif
}

static void hmac_SHA256_step(uint8_t *output, uint8_t mask,
			const uint8_t *key, const int key_len,
			const uint8_t *data, const int data_len)
{
	struct sha256_ctx ctx;
	uint8_t *tmp;
	int i;

	memset(ctx.block, mask, SHA256_BLOCK_SIZE);
	for (i = 0; i < key_len; i++)
		ctx.block[i] ^= key[i];

	SHA256_init_1b(&ctx, ctx.wblock);
	SHA256_update(&ctx, data, data_len);
	tmp = SHA256_final(&ctx);
	memcpy(output, tmp, SHA256_DIGEST_SIZE);
}

void hmac_SHA256(uint8_t *output, const uint8_t *key, const int key_len,
		 const uint8_t *message, const int message_len)
{
	/* This code does not support key_len > block_size. */
	ASSERT(key_len <= SHA256_BLOCK_SIZE);

	/*
	 * i_key_pad = key (zero-padded) ^ 0x36
	 * output = hash(i_key_pad || message)
	 * (Use output as temporary buffer)
	 */
	hmac_SHA256_step(output, 0x36, key, key_len, message, message_len);

	/*
	 * o_key_pad = key (zero-padded) ^ 0x5c
	 * output = hash(o_key_pad || output)
	 */
	hmac_SHA256_step(output, 0x5c,
			 key, key_len, output, SHA256_DIGEST_SIZE);
}

/*
 * Console command test
 */
#ifdef CONFIG_CMD_SHA256_TEST

/*
 * 56 bytes is corner case where padding algorithm should add a second
 * 64 byte block:
 * [56 byte message] || 0x80 || [7 0x00 bytes]
 * [56 bytes of 0x00] || [8 byte message bit length MSB first]
 */
#define SHA256_TEST_PATTERN1_LEN 56
const uint8_t test_pattern1[SHA256_TEST_PATTERN1_LEN+1] =
	"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

/*
 * FIPS 180-4 documented result is
 * 248D6A61 D20638B8 E5C02693 0C3E6039 A33CE459 64FF2167 F6ECEDD4 19DB06C1
 * This is a byte stream laid out in memory low(left) to high(right)
 */
const uint8_t __aligned(4)
test_pattern1_sha256[SHA256_DIGEST_SIZE] = {
	0x24, 0x8D, 0x6A, 0x61, 0xD2, 0x06, 0x38, 0xB8,
	0xE5, 0xC0, 0x26, 0x93, 0x0C, 0x3E, 0x60, 0x39,
	0xA3, 0x3C, 0xE4, 0x59, 0x64, 0xFF, 0x21, 0x67,
	0xF6, 0xEC, 0xED, 0xD4, 0x19, 0xDB, 0x06, 0xC1
};

/*
 * openssl rand 96 > rand96.bin
 * One block (64-bytes) plus 32 remaining bytes.
 * [first 64 bytes of message]
 * [remaining 32 bytes of message] || 0x80 || [(54-33)=21 0x00 bytes] ||
 *	[8 bytes message bit length MSB first]
 */
#define SHA256_TEST_PATTERN2_LEN 96
const uint8_t __aligned(4)
test_pattern2[SHA256_TEST_PATTERN2_LEN] = {
	0xBF, 0xA6, 0xC8, 0xF0, 0xFD, 0x5C, 0xE5, 0x4A, 0x5F, 0x67,
	0x67, 0x35, 0x66, 0x39, 0x1E, 0x44, 0xA8, 0x92, 0x92, 0x5A,
	0xEB, 0xAD, 0xAF, 0x6A, 0x71, 0x04, 0x58, 0x1C, 0x2E, 0xDA,
	0xEE, 0x25, 0x92, 0x6D, 0xB8, 0x56, 0x13, 0x5B, 0xB4, 0x4E,
	0x6B, 0x3E, 0x7E, 0x87, 0x02, 0x5F, 0xCA, 0x88, 0x50, 0x0A,
	0xBB, 0xFA, 0x8B, 0x7A, 0xFC, 0x95, 0xEC, 0x2D, 0xB6, 0xB8,
	0xD9, 0x16, 0x72, 0x75, 0xEB, 0x67, 0x41, 0x31, 0x98, 0x4A,
	0x97, 0xFB, 0x5F, 0xD1, 0xBE, 0xB0, 0x70, 0xE7, 0x67, 0xC9,
	0xEA, 0xB1, 0x3C, 0x0C, 0xB4, 0xB2, 0x26, 0x49, 0xC7, 0x26,
	0xA7, 0xD7, 0x19, 0xF2, 0xC8, 0x8B
};

/*
 * openssl dgst -sha256 -out hex96.txt rand96.bin
 * 2c99b17b91cbbc4f3df6b502d6a2fd618cde5003ca97d1696962d7771e301550
 */
const uint8_t __aligned(4)
test_pattern2_sha256[SHA256_DIGEST_SIZE] = {
	0x2c, 0x99, 0xb1, 0x7b, 0x91, 0xcb, 0xbc, 0x4f,
	0x3d, 0xf6, 0xb5, 0x02, 0xd6, 0xa2, 0xfd, 0x61,
	0x8c, 0xde, 0x50, 0x03, 0xca, 0x97, 0xd1, 0x69,
	0x69, 0x62, 0xd7, 0x77, 0x1e, 0x30, 0x15, 0x50
};

/*
 * Algorithm should compute:
 * Two 64-byte blocks (first 128 bytes of message)
 * Third block contains:
 *   [remaining 60 bytes of message] || 0x80 || [3 0x00 bytes]
 * Fourth block contains:
 *   [56 0x00 bytes] || [8 bytes message bit length MSB first]
 */
#define SHA256_TEST_PATTERN3_LEN 188
const uint8_t __aligned(4)
test_pattern3[SHA256_TEST_PATTERN3_LEN] = {
	0x98, 0x07, 0x86, 0x20, 0x61, 0x37, 0x0A, 0xEE, 0x52, 0xC3,
	0x01, 0x0C, 0x19, 0xB1, 0x5B, 0x83, 0x8F, 0x2B, 0x9F, 0x57,
	0x53, 0x61, 0x3A, 0xBE, 0x15, 0xBF, 0x54, 0x48, 0xFE, 0x8D,
	0x9A, 0x89, 0x69, 0xC9, 0x54, 0x57, 0x66, 0x4E, 0x39, 0xCE,
	0x0C, 0x1D, 0x7A, 0x71, 0xE2, 0xF0, 0x6F, 0x36, 0xBC, 0x1E,
	0xB4, 0xA1, 0xCF, 0xAD, 0x93, 0xAE, 0x65, 0x45, 0xC9, 0x4E,
	0x1A, 0x66, 0x08, 0x1A, 0x78, 0x04, 0x58, 0x2A, 0xF4, 0xEA,
	0x65, 0x28, 0xC3, 0x00, 0x77, 0x5B, 0x5B, 0x2B, 0x6A, 0x71,
	0xB1, 0xD2, 0x0B, 0xFF, 0x81, 0xB5, 0xD9, 0xB1, 0x07, 0x0D,
	0xE5, 0xF5, 0x9C, 0xAE, 0x91, 0x06, 0xD7, 0x43, 0x2A, 0x95,
	0x1C, 0xD4, 0xA0, 0xDC, 0xE8, 0x7D, 0x0F, 0xD9, 0x8D, 0x83,
	0xAA, 0x06, 0x53, 0x9F, 0x63, 0xDB, 0x0A, 0x6B, 0x4F, 0x2B,
	0x45, 0x97, 0xC4, 0xC3, 0xE1, 0x1C, 0xAB, 0x81, 0xD1, 0x36,
	0x41, 0x03, 0xFC, 0x00, 0x69, 0x39, 0x97, 0x56, 0x4C, 0xCA,
	0x9E, 0xFD, 0x4A, 0xA9, 0xB2, 0x64, 0x08, 0xBB, 0x03, 0x77,
	0x53, 0x28, 0xA1, 0xB3, 0x6A, 0x8C, 0x3C, 0x13, 0x49, 0x30,
	0x77, 0xE9, 0x3B, 0xAC, 0xF2, 0x07, 0x9E, 0x4B, 0x7A, 0x5C,
	0x9A, 0x0F, 0x90, 0xD8, 0xE2, 0xEB, 0xA3, 0x17, 0x32, 0x00,
	0x08, 0x31, 0x41, 0x85, 0xB2, 0x6A, 0x1C, 0xD4
};

/*
 * openssl rand 188 > rand188.bin
 * openssl dgst -sha256 -out hex188.txt rand188.bin
 * fafafd83c8221818d7fb80f5b9bf9e130fde4e83801d8b42beef4aefe0d36fcb
 */
const uint8_t __aligned(4)
test_pattern3_sha256[SHA256_DIGEST_SIZE] = {
	0xfa, 0xfa, 0xfd, 0x83, 0xc8, 0x22, 0x18, 0x18,
	0xd7, 0xfb, 0x80, 0xf5, 0xb9, 0xbf, 0x9e, 0x13,
	0x0f, 0xde, 0x4e, 0x83,	0x80, 0x1d, 0x8b, 0x42,
	0xbe, 0xef, 0x4a, 0xef, 0xe0, 0xd3, 0x6f, 0xcb
};


static struct sha256_ctx sha256_ctx_test;


/*
 * from terminal sha256hw_init no parameters
 * R0 = argc = 1
 * R1 = **argv = 0x11bb80
 *
 * paramter 1 = operation
 *   0 = power, param2 = 0(off), 1(on)
 *   1 = check test pattern 1
 *   2 = check test pattern 2
 *   3 = check test pattern 3
 *   4 = init
 *   5 = update, param2 = num bytes, para3 = pointer to bytes
 *   6 = final
 *
 */
static int cmd_sha256_test(int argc, char **argv)
{
	char *e;
	int t;
	uint32_t i;
	uint8_t *pdigest;
	const uint8_t *ptest;
	const uint8_t *pexp;
	uint32_t msg_byte_len;

	pdigest = NULL;
	ptest = NULL;
	pexp = NULL;
	msg_byte_len = 0;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	t = strtoi(argv[1], &e, 0);
	switch (t) {
	case 0:
		memset(&sha256_ctx_test, 0, sizeof(struct sha256_ctx));
#ifdef CONFIG_SHA256_HW
		chip_sha_hw_enable(1);
		return chip_sha_hw_is_enabled();
#endif
		return EC_SUCCESS;
	case 1:
		ptest = test_pattern1;
		pexp = test_pattern1_sha256;
		msg_byte_len = SHA256_TEST_PATTERN1_LEN;
		break;
	case 2:
		ptest = test_pattern2;
		pexp = test_pattern2_sha256;
		msg_byte_len = SHA256_TEST_PATTERN2_LEN;
		break;
	case 3:
		ptest = test_pattern3;
		pexp = test_pattern3_sha256;
		msg_byte_len = SHA256_TEST_PATTERN3_LEN;
		break;
	case 4:
		SHA256_init(&sha256_ctx_test);
		SHA256_update(&sha256_ctx_test, &test_pattern1[0], 25);
		SHA256_update(&sha256_ctx_test, &test_pattern1[25], 25);
		SHA256_update(&sha256_ctx_test, &test_pattern1[50], 6);
		pdigest = SHA256_final(&sha256_ctx_test);
		if (memcmp(pdigest, test_pattern1_sha256,
			   SHA256_DIGEST_SIZE) != 0) {
			CPRINTF("SHA256 Test 4 FAIL - digest mismatch\n");
			return EC_ERROR_CRC;
		}
		break;
	case 5:
		SHA256_init(&sha256_ctx_test);
		for (i = 0; i < SHA256_TEST_PATTERN3_LEN/10; i++) {
			SHA256_update(&sha256_ctx_test,
				      &test_pattern3[i*10], 10);
		}
		ptest = test_pattern3 + SHA256_TEST_PATTERN3_LEN -
				(SHA256_TEST_PATTERN3_LEN % 10);
		SHA256_update(&sha256_ctx_test, ptest,
			      (SHA256_TEST_PATTERN3_LEN % 10));
		pdigest = SHA256_final(&sha256_ctx_test);
		if (memcmp(pdigest, test_pattern3_sha256,
			   SHA256_DIGEST_SIZE) != 0) {
			CPRINTF("SHA256 Test 5 FAIL - digest mismatch\n");
			return EC_ERROR_CRC;
		}
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	if (msg_byte_len != 0) {
		SHA256_init(&sha256_ctx_test);
		SHA256_update(&sha256_ctx_test, ptest, msg_byte_len);
		pdigest = SHA256_final(&sha256_ctx_test);
		if (memcmp(pdigest, pexp, SHA256_DIGEST_SIZE) != 0) {
			CPRINTF("SHA256 Test %d FAIL - digest mismatch\n", t);
			return EC_ERROR_CRC;
		}


	}

	CPRINTF("SHA256 Test %d PASS\n", t);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sha256, cmd_sha256_test,
			"0/init 1/test1 2/test2 3/test3",
			"SHA256 test");

#endif /* #ifdef CONFIG_SHA256_TEST */
