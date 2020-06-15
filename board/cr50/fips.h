/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __EC_BOARD_CR50_FIPS_H__
#define __EC_BOARD_CR50_FIPS_H__

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif
/* Signals start on the left, errors on the right. */
enum fips_status {
	/* FIPS_MODE_REQUESTED caches FWMP policy */
	FIPS_MODE_ACTIVE = 1U << 31,
	FIPS_POWER_UP_TEST_DONE = 1U << 30,
	/* default value */
	FIPS_UNINITIALIZED = 0,
	FIPS_FATAL_TRNG_RCT = 1 << 1,
	FIPS_FATAL_TRNG_APT = 1 << 2,
	FIPS_FATAL_TRNG_OTHER = 1 << 3,
	FIPS_FATAL_SHA256 = 1 << 4,
	FIPS_FATAL_HMAC_SHA256 = 1 << 5,
	FIPS_FATAL_HMAC_DRBG = 1 << 6,
	FIPS_FATAL_ECDSA = 1 << 7,
	FIPS_FATAL_RSA2048 = 1 << 8,
	FIPS_FATAL_AES256 = 1 << 9,
	FIPS_FATAL_OTHER = 1 << 15,
	FIPS_ERROR_MASK = 0xffff,
	FIPS_RFU_MASK = 0x7fff0000
};

/* simulate error in specific block */
enum fips_break {
	FIPS_NO_BREAK = 0,
	FIPS_BREAK_TRNG = 1,
	FIPS_BREAK_SHA256 = 2,
	FIPS_BREAK_HMAC_SHA256 = 3,
	FIPS_BREAK_HMAC_DRBG = 4,
	FIPS_BREAK_ECDSA = 5,
	FIPS_BREAK_AES256 = 6
};
extern uint8_t fips_break_cmd;
/* return status of operations */
enum fips_status fips_status(void);
/**
 * Crypto is enabled when either FIPS mode is not enforced,
 * or if it is enforced and in good health
 * @returns non-zero if crypto can be executed.
 */
bool fips_crypto_allowed(void);
void fips_set_status(enum fips_status status);
void _fips_throw_err(enum fips_status err, const char *func, int line);
/* update FIPS error status */
#define fips_throw_err(err) _fips_throw_err(err, __func__, __LINE__)
#ifdef __cplusplus
}
#endif
#endif /* __EC_BOARD_CR50_FIPS_H__ */
