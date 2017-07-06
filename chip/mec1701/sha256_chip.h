/* Copyright 2017 The Chromium OS Authors. All rights reserved
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Register map for MEC17xx processor
 */
/** @file sha256_chip.h
 *MEC17xx Device SHA hardware accelerator
 */
/** @defgroup MEC17xx crypto
 */

#ifndef _SHA256_CHIP_H
#define _SHA256_CHIP_H

#include <stdint.h>
#include <stddef.h>

/* EC common sha256 context data structure definition */
#include "sha256.h"

#define SHA256_DIGEST_BYTELEN	(32ul)
#define SHA256_DIGEST_WORDLEN	(8ul)

/*
 * SHA-1 and SHA-256 use the same block length = 64 bytes
 * Both also use the same message bit length size of 8 bytes.
 */

#define SHA256_BLOCK_BYTELEN	(64ul)
#define SHA256_BLOCK_WORDLEN	(16ul)


/* Maximum SHA-1 and SHA-256 byte length */
#define SHA256_MSG_LEN_MAX  (0x1FFFFFFFFFFFFFFFULL)




#ifdef __cplusplus
extern "C" {
#endif

/* Place any C interfaces here */

extern const uint32_t rom_sha256_h0[SHA256_DIGEST_WORDLEN];

int chip_sha_hw_enable(int enable_block);
int chip_sha_hw_is_enabled(void);

/*
 * Same API as software version in common/sha256.c
 * Due to use of hardware accelerator we use a mutex.
 */
void chip_sha256_init(struct sha256_ctx *ctx);
void chip_sha256_update(struct sha256_ctx *ctx,
		const uint8_t *data, uint32_t len);
uint8_t *chip_sha256_final(struct sha256_ctx *ctx);

void chip_hmac_SHA256(uint8_t *output, const uint8_t *key,
		const int key_len, const uint8_t *message,
		const int message_len);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _SHA256_CHIP_H */
/**   @}
 */

