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


static int rom_sha_raw_update_wrap(const uint32_t *pdata, uint32_t *pdigest,
					uint16_t nblocks)
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


void chip_sha256_init_1b(struct sha256_ctx *ctx, const uint32_t *data)
{
	int i;

	for (i = 0; i < 8; i++)
		ctx->h[i] = rom_sha256_h0[i];

	chip_sha256_transform(ctx, data, 1);

	ctx->len = 0;
	ctx->tot_len = SHA256_BLOCK_SIZE;
}

int chip_sha256_init(struct sha256_ctx *ctx)
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

	return EC_SUCCESS;
}



/*
 * Copies up to SHA256_BLOCK_SIZE bytes from data into context block.
 * if context block length + length of data is less than SHA256_BLOCK_SIZE we
 * can't do any computation. Therefore update context length and return.
 * else current context length plus length of data is >= SHA256_BLOCK_SIZE
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
void chip_sha256_update(struct sha256_ctx *ctx, const uint8_t *data,
			uint32_t len)
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
 * Original code.
 * Experiment with 121 byte message
 * ctx->len = 57 with the 57 bytes in ctx->block
 * ctx->tot_len = 64
 * block_nb = (1 + (64 - 9)) < (57 % 64) -> 56 < 57 = 1
 * if ctx->len was 43 then block_nb = 0
 * pm_len = 1 << 6 = 64
 * if ctx->len was 43 then pm_len = 0
 * fill ctx->block with zero from index ctx->len(57) length = 7 bytes
 * set ctx->block[ctx->len] = 0x80
 * !!! BUG !!! FIPS Hash spec. states for message M of length l bits:
 * append 1 bit to end of message followed by k zero bits where
 * (l + 1 + k) mod 512 = 448 mod 512 and k is smallest non-negative solution.
 * l = 121 * 8 = 968
 * 448 + (512 - (57 * 8)) - 1 = 448 + 56 = 503
 * (968 + 1 + k) mod 512 = 448 mod 512
 * 969 mod 512 + k mod 512 = 448 mod 512
 * k mod 512 = (448 - 969) mod 512 = -521 mod 512
 * Keep adding 512 to -521 until result is > 0 and less than 512
 * -521 + 512 + 512 = 503
 * k mod 512 = 503 mod 512
 * k = 503 bits
 * This original implementation is incorrect.
 * For a message length of 121 bytes (968 bits)
 * it only adds 7 bytes (56 bits) of 0's.
 */

/*
 * ctx->h contains intermedia hash value
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

