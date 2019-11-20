/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_FIPS_DRBG_H
#define __EC_BOARD_CR50_FIPS_DRBG_H

#include <stddef.h>
#include <string.h>

#include "common.h"
#include "util.h"

#include "cryptoc/p256.h"
#include "cryptoc/sha.h"
#include "cryptoc/sha256.h"
#include "cryptoc/sha384.h"
#include "cryptoc/sha512.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * store full (condensed) entropy into buffer.
 */
void get_entropy(void *buffer, size_t len);

/**
 * get 32 bits of full (condensed) entropy.
 */
uint32_t get_entropy32(void);

/* initialize cr50-wide DRBG replacing rand */
void fips_drbg_init(void);

/* mark cr50-wide DRBG as not initialized */
void fips_drbg_init_clear(void);

void fips_rand_bytes(void *buffer, size_t len);

uint32_t fips_rand(void);


#ifdef __cplusplus
}
#endif

#endif  /* ! __EC_BOARD_CR50_FIPS_DRBG_H */
