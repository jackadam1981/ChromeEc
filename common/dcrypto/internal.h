/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Crypto wrapper library for CR50.
 */
#ifndef EC_BOARD_CR50_DCRYPTO_INTERNAL_H_
#define EC_BOARD_CR50_DCRYPTO_INTERNAL_H_

#include <inttypes.h>

#define CTRL_CTR_BIG_ENDIAN (__BYTE_ORDER__  == __ORDER_BIG_ENDIAN__)
#define CTRL_ENABLE         1
#define CTRL_ENCRYPT        1
#define CTRL_NO_SOFT_RESET  0

struct HASH_CTX;      /* Forward declaration. */

typedef struct HASH_VTAB {
	void (* const init)(struct HASH_CTX *, uint32_t);
	void (* const update)(struct HASH_CTX *, const uint8_t *, uint32_t);
	const uint8_t *(* const final)(struct HASH_CTX *);
	const uint8_t *(* const hash)(const uint8_t *, uint32_t, uint8_t *);
	uint32_t size;
} HASH_VTAB;

typedef struct HASH_CTX {
	const HASH_VTAB *vtab;
	uint64_t count;
	uint8_t buf[64];
	uint32_t state[8];  /* Space sufficient for SHA2. */
} HASH_CTX;

enum sha_mode {
	SHA1_MODE = 0,
	SHA256_MODE = 1
};

extern int hw_available;

void dcrypto_sha_init(enum sha_mode mode);
void dcrypto_sha_update(HASH_CTX *unused, const uint8_t *data, uint32_t n);
void dcrypto_sha_wait(enum sha_mode mode, uint32_t *digest);
void dcrypto_sha_hash(enum sha_mode mode, const uint8_t *data,
		uint32_t n, uint8_t *digest);

#endif /* ! EC_BOARD_CR50_DCRYPTO_INTERNAL_H_ */
