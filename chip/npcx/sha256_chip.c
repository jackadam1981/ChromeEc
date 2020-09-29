/*
 * Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* SHA256 module for Chrome EC */
#include "common.h"
#include "sha256.h"
#include "util.h"

enum ncl_status {
	NCL_STATUS_OK,
	NCL_STATUS_FAIL,
	NCL_STATUS_INVALID_PARAM,
	NCL_STATUS_PARAM_NOT_SUPPORTED,
	NCL_STATUS_SYSTEM_BUSY,
	NCL_STATUS_AUTHENTICATION_FAIL,
	NCL_STATUS_NO_RESPONSE,
	NCL_STATUS_HARDWARE_ERROR,
};

enum ncl_sha_type {
	NCL_SHA_TYPE_2_256 = 0,
	NCL_SHA_TYPE_2_384 = 1,
	NCL_SHA_TYPE_2_512 = 2,
	NCL_SHA_TYPE_NUM
};

/*
 * The base address of the table that holds the function pointer for each
 * SHA256 API in ROM.
 */
#define NCL_SHA_BASE_ADDR          0x00000100UL
#define NCL_SHA_OFFSET(x)          (NCL_SHA_BASE_ADDR + (x))

#define NCL_SHA_GET_CTX_SIZE_PTR (*((uint32_t *)NCL_SHA_OFFSET(0x00)))
#define NCL_SHA_INIT_CTX_PTR     (*((uint32_t *)NCL_SHA_OFFSET(0x04)))
#define NCL_SHA_FINALIZE_CTX_PTR (*((uint32_t *)NCL_SHA_OFFSET(0x08)))
#define NCL_SHA_INIT_PTR         (*((uint32_t *)NCL_SHA_OFFSET(0x0C)))
#define NCL_SHA_START_PTR        (*((uint32_t *)NCL_SHA_OFFSET(0x10)))
#define NCL_SHA_UPDATE_PTR       (*((uint32_t *)NCL_SHA_OFFSET(0x14)))
#define NCL_SHA_FINISH_PTR       (*((uint32_t *)NCL_SHA_OFFSET(0x18)))
#define NCL_SHA_POWER_PTR        (*((uint32_t *)NCL_SHA_OFFSET(0x20)))
#define NCL_SHA_RESET_PTR        (*((uint32_t *)NCL_SHA_OFFSET(0x24)))

/* Get the SHA context size required by SHA APIs. */
typedef uint32_t (*ncl_sha_get_context_size)(void);
/* Initial SHA context. */
typedef enum ncl_status (*ncl_sha_init_context)(void *ctx);
/* Finalize SHA context. */
typedef enum ncl_status (*ncl_sha_finalize_context)(void *ctx);
/* Initiate the SHA hardware module and setups needed parameters. */
typedef enum ncl_status (*ncl_sha_init)(void *ctx);
/*
 * Prepare the context buffer for a SHA calculation -  by loading the initial
 * SHA-256/384/512 parameters.
 */
typedef enum ncl_status (*ncl_sha_start)(void *ctx, enum ncl_sha_type type);
/*
 * Updates the SHA calculation with the additional data. When the function
 * returns, the hardware and memory buffer shall be ready to accept new data
 * buffers for SHA calculation and changes to the data in data buffer should
 * no longer effect the SHA calculation.
 */
typedef enum ncl_status (*ncl_sha_update)(void *ctx, const uint8_t *data,
						uint32_t Len);
/* Return the SHA result (digest.) */
typedef enum ncl_status (*ncl_sha_finish)(void *ctx, uint8_t *hashDigest);
/* Power on/off the SHA module. */
typedef enum ncl_status (*ncl_sha_power)(void *ctx, uint8_t enable);
/* Reset the SHA hardware and terminate any in-progress operations. */
typedef enum ncl_status (*ncl_sha_reset)(void *ctx);

#define SHA_GET_CTX_SIZE() \
		((ncl_sha_get_context_size) NCL_SHA_GET_CTX_SIZE_PTR)()
#define SHA_INIT_CTX(c) \
		((ncl_sha_init_context) NCL_SHA_INIT_CTX_PTR)(c)
#define SHA_FINALIZE_CTX(c) \
		((ncl_sha_finalize_context) NCL_SHA_FINALIZE_CTX_PTR)(c)
#define SHA_INIT(c) \
		((ncl_sha_init) NCL_SHA_INIT_PTR)(c)
#define SHA_START(c, t) \
		((ncl_sha_start) NCL_SHA_START_PTR)(c, t)
#define SHA_UPDATE(c, d, l) \
		((ncl_sha_update) NCL_SHA_UPDATE_PTR)(c, d, l)
#define SHA_FINISH(c, h) \
		((ncl_sha_finish) NCL_SHA_FINISH_PTR)(c, h)
#define SHA_POWER(c, e) \
		((ncl_sha_power) NCL_SHA_POWER_PTR)(c, e)
#define SHA_RESET(c) \
		((ncl_sha_reset) NCL_SHA_RESET_PTR)(c)

void SHA256_init(struct sha256_ctx *ctx)
{
	SHA_INIT_CTX(ctx->handle);
	SHA_POWER(ctx->handle, 1);
	SHA_INIT(ctx->handle);
	SHA_RESET(ctx->handle);
	SHA_START(ctx->handle, NCL_SHA_TYPE_2_256);
}

void SHA256_update(struct sha256_ctx *ctx, const uint8_t *data, uint32_t len)
{
	SHA_UPDATE(ctx->handle, data, len);
}

void SHA256_abort(struct sha256_ctx *ctx)
{
	SHA_RESET(ctx->handle);
	SHA_POWER(ctx->handle, 0);
	SHA_FINALIZE_CTX(ctx->handle);
}

uint8_t *SHA256_final(struct sha256_ctx *ctx)
{
	SHA_FINISH(ctx->handle, ctx->buf);
	SHA_POWER(ctx->handle, 0);
	SHA_FINALIZE_CTX(ctx->handle);
	return ctx->buf;
}

static void hmac_SHA256_step(uint8_t *output, uint8_t mask,
			const uint8_t *key, const int key_len,
			const uint8_t *data, const int data_len)
{
	struct sha256_ctx hmac_ctx;
	uint8_t *key_pad = hmac_ctx.buf;
	uint8_t *tmp;
	int i;

	memset(key_pad, mask, SHA256_BLOCK_SIZE);
	for (i = 0; i < key_len; i++)
		key_pad[i] ^= key[i];

	SHA256_init(&hmac_ctx);
	SHA256_update(&hmac_ctx, key_pad, SHA256_BLOCK_SIZE);
	SHA256_update(&hmac_ctx, data, data_len);
	tmp = SHA256_final(&hmac_ctx);
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
	hmac_SHA256_step(output, 0x5c, key, key_len, output,
				SHA256_DIGEST_SIZE);
}
