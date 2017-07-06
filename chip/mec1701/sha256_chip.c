/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "sha256.h"
#include "util.h"
#include "registers.h"
#include "task.h"
#ifndef LFW
#include "timer.h"
#endif

#include "rom_api_chip.h"
#include "sha256_chip.h"



#define SHA256_BLOCK_BYTELEN	(64ul)
#define SHA256_BLOCK_WORDLEN	(16ul)
#define SHA256_BLOCK_SIZE_SHIFT (6)
#define SHA256_BLOCK_SIZE_MASK  ((SHA256_BLOCK_BYTELEN) - 1)

#define MEC17XX_HASH_GIRQ           (16)

/*
 * Computation times
 * 64 cycles per 64 byte chunk for SHA-256.
 * 80 cycles per 128 byte chunk for SHA-512.
 * This does not take into account time to read data into Hash engine.
 * AHB @ 48MHz assuming no contention with other masters.
 * if Hash block uses AHB burst mode then
 * 64 bytes = 64/4 + 3 AHB clocks = 0.4 us
 * else 64 bytes = (64/4) * 3 = 48 AHB clocks = 1 us
 * SHA-256 one block total = 64 + 48(worst case) = 112 cycles or 2.3 us
 * SHA-512 one block total = 80 + (128/4) * 3 = 176 cycles or 3.7 us
 *
 */
#define HASH_BLOCK_TRANSFER_TIMEOUT_US          (16)
#define HASH_BLOCK_TRANSFER_POLL_INTERVAL_US    (32)


#define BYTE_REVERSE_WORD(w) ((((uint32_t)(w)>>24)&0xFFul) + \
				(((uint32_t)(w)&0x00FF0000ul)>>8) + \
				(((uint32_t)(w)&0x0000FF00ul)<<8) + \
				(((uint32_t)(w)&0xFFul)<<24))



#ifndef LFW
static struct mutex hash_mutex;
#endif


static int rom_sha256_raw_result(uint8_t rc)
{
	int result;

	switch (rc) {
	case 0:
		result = EC_SUCCESS;
		break;
	case 0x80:
		result = EC_ERROR_BUSY;
		break;
	case 0x81: /* pointer param is NULL */
		result = EC_ERROR_INVAL;
		break;
	default:
		result = EC_ERROR_UNKNOWN;
	}

	return result;
}


static int rom_sha_raw_update_wrap(const uint32_t *pdata,
		uint32_t *pdigest, uint16_t nblocks)
{
	uint8_t rc;

	rc = rom_sha256_raw_update(pdata, pdigest, nblocks);

	return rom_sha256_raw_result(rc);
}


#ifndef LFW
/*
 * NOTE: if LFW is defined the LFW code must supply its version
 * of this function.
 */
static int hash_wait(uint32_t block_nb)
{
	timestamp_t deadline;
	uint32_t htm;

	htm = ((HASH_BLOCK_TRANSFER_TIMEOUT_US) * block_nb);
	deadline.val = get_time().val + htm;

	while (rom_hash_busy()) {
		if (timestamp_expired(deadline, NULL))
			return EC_ERROR_TIMEOUT;
		usleep(HASH_BLOCK_TRANSFER_POLL_INTERVAL_US);
	}
	return EC_SUCCESS;
}
#endif /* #ifndef LFW */


static int chip_sha256_transform(struct sha256_ctx *ctx,
				const uint32_t *pblocks,
				unsigned int block_nb)
{
	int rc;

#ifndef LFW
	mutex_lock(&hash_mutex);
#endif

	trace11(0, SHA, 0, "SHA256_Transform: pblock  = 0x%08x",
		(uint32_t)pblocks);
	trace11(0, SHA, 0, "SHA256_Transform: nblocks = 0x%08x", block_nb);

	rom_hash_iclr();
	rc = rom_sha_raw_update_wrap(pblocks, ctx->h, block_nb);
	if (rc == EC_SUCCESS) {
		trace0(0, SHA, 0, "Hash Wait");
		rc = hash_wait(block_nb);
		trace0(0, SHA, 0, "Hash Wait Done");
	}

#ifndef LFW
	mutex_unlock(&hash_mutex);
#endif

	return rc;
}


int chip_sha_hw_enable(int enable_block)
{
	if (enable_block) {
		rom_aes_sha_power(1);
		rom_aes_sha_reset();
	} else {
		rom_aes_sha_reset();
		rom_aes_sha_power(0);
	}

	return EC_SUCCESS;
}

int chip_sha_hw_is_enabled(void)
{
	if (MEC17XX_PCR_SLP_EN3 & MEC17XX_PCR_SLP_EN3_AESHASH)
		return EC_ERROR_NOT_POWERED;

	return EC_SUCCESS;
}

void chip_sha256_h0_fill(uint32_t *digest)
{
	uint32_t i;

	if (digest != NULL)
		for (i = 0; i < SHA256_DIGEST_WORDLEN; i++)
			digest[i] = rom_sha256_h0[i];
}


static void SHA256_init_1b(struct sha256_ctx *ctx, const uint32_t *data)
{
	int i;

	for (i = 0; i < 8; i++)
		ctx->h[i] = rom_sha256_h0[i];

	chip_sha256_transform(ctx, data, 1);

	ctx->len = 0;
	ctx->tot_len = SHA256_BLOCK_SIZE;
}

void chip_sha256_init(struct sha256_ctx *ctx)
{
	memset(ctx, 0, sizeof(struct sha256_ctx));

#ifndef LFW
	mutex_lock(&hash_mutex);
#endif

	rom_sha256_raw_init(&ctx->h[0]);
	rom_hash_iclr();

	ctx->len = 0;
	ctx->tot_len = 0;

#ifndef LFW
	mutex_unlock(&hash_mutex);
#endif
}



/*
 * Copies up to SHA256_BLOCK_SIZE bytes from data into context block.
 * if context block length + length of data is less than SHA256_BLOCK_SIZE
 * we can't do any computation. Therefore update context length and return.
 * If current context length plus length of data is >= SHA256_BLOCK_SIZE
 * then start computation on (context length + data length) % 64 blocks.
 * Update context length with remaining bytes.
 *
 * Alternatives:
 * 1. Copy data to fill ctx->block, hash ctx->block, repeat
 *    Simple but overhead of copying all data into ctx->block
 *
 * 2. if len(ctx->block) == 0 and address(data) aligned >= 4 bytes
 *          and size(data) >= 64 bytes
 *      Start Hash engine on data for (len(data) >> 6) blocks
 *      copy remaining data into ctx->block
 *    else
 *      Copy data to fill ctx->block and start engine on ctx->block
 *      if next data is aligned and length >= 64 start engine on data
 *      copy remaining data into ctx->block
 *
 *
 */

/* solution 1 */
void chip_sha256_update(struct sha256_ctx *ctx,
		const uint8_t *data, uint32_t len)
{
	uint32_t block_nb;
	uint32_t new_len, rem_len, tmp_len;
	const uint8_t *shifted_data;

	trace12(0, SHA, 0, "chip_sha256_update: data=0x%08x  len=0x%08x",
		(uint32_t)data, len);
	trace12(0, SHA, 0,
		"chip_sha256_update: ctx.len=0x%08x ctx.tot_len=0x%08x",
		ctx->len, ctx->tot_len);

	/* space available in context block buffer */
	tmp_len = SHA256_BLOCK_SIZE - ctx->len;
	/* copy up to space available bytes */
	rem_len = len < tmp_len ? len : tmp_len;
	memcpy(&ctx->block[ctx->len], data, rem_len); /* copy */

	if (ctx->len + len < SHA256_BLOCK_SIZE) {
		ctx->len += len;
		return;
	}

	new_len = len - rem_len;    /* amount of data remaining */
	/* number of 64-byte blocks remaining, may be less than new_len */
	block_nb = new_len  >> SHA256_BLOCK_SIZE_SHIFT;

	chip_sha256_transform(ctx, &ctx->wblock[0], 1);

	shifted_data = data + rem_len; /* is this pointer >= 32-bit aligned? */

	/* block_nb may be 0 meaning less than 64 bytes are remaining */
	if (block_nb) {
		if (((uint32_t)shifted_data & 0x03) == 0) {
			chip_sha256_transform(ctx,
				(const uint32_t *)shifted_data,
				block_nb);
			shifted_data += (block_nb << 6);
		} else { /* no, pointer not aligned */
			/*
			 * copy data 64 bytes at a time into aligned
			 * ctx->block and run engine
			 */
			while (block_nb--) {
				memcpy(&ctx->block[0], shifted_data,
					SHA256_BLOCK_SIZE);
				chip_sha256_transform(ctx, &ctx->wblock[0], 1);
				shifted_data += SHA256_BLOCK_SIZE;
			}
		}
	}

	rem_len = new_len & SHA256_BLOCK_SIZE_MASK;
	memcpy(&ctx->block[0], shifted_data, rem_len);

	ctx->len = rem_len;
	ctx->tot_len += (block_nb + 1) << 6;
}


/*
 * ctx->h contains intermediate hash value
 * when computation(s) done copy ctx->h into ctx->buf
 * ctx->block is 2 * 64 = 128 bytes in length to
 * handle corner case padding of messages with
 * remaining bytes >= 56.
 */
uint8_t *chip_sha256_final(struct sha256_ctx *ctx)
{
	uint64_t len_b;
	uint32_t nb;
	uint32_t i;

	trace12(0, SHA, 0,
		"chip_sha256_final: ctx.len=0x%08x ctx.tot_len=0x%08x",
		ctx->len, ctx->tot_len);

	nb = 1 + (ctx->len > (SHA256_BLOCK_SIZE - 9));

	memset(&ctx->block[0] + ctx->len, 0, ((nb << 6) - ctx->len));

	ctx->block[ctx->len] = 0x80;

	/* Store bit length most significant byte first in last 8 bytes */
	len_b = (ctx->tot_len + ctx->len) << 3;
	for (i = 1; i <= 8; i++) {
		ctx->block[(nb << 6) - i] = (uint8_t)(len_b & 0xff);
		len_b >>= 8;
	}

	chip_sha256_transform(ctx, &ctx->wblock[0], nb);

	/* copy final result into ctx->buf */
	for (i = 0; i < 8; i++)
		ctx->wbuf[i] = ctx->h[i];

	return &ctx->buf[0];
}

/* HMAC-SHA256 routines */
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

void chip_hmac_SHA256(uint8_t *output, const uint8_t *key,
		const int key_len, const uint8_t *message,
		const int message_len)
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
#ifdef CONFIG_CMD_SHA256_HW_TEST

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
 * parameter 1 = operation
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

#endif /* #ifdef CONFIG_SHA256_HW_TEST */

