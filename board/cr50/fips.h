/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_FIPS_H__
#define __EC_BOARD_CR50_FIPS_H__
#ifdef __cplusplus
extern "C" {
#endif

/* Signals start on the left, errors on the right. */
enum fips_err {
	FIPS_TRNG_TEST_COMPLETED = 1U << 31,
	FIPS_KAT_TEST_PASSED = 1U << 30,
	FIPS_DRBG_INITIALIZED = 1U << 29,
	FIPS_INITIALIZED = 1U << 28,

	FIPS_UNINITIALIZED = 0,
	FIPS_FATAL_TRNG = 1 << 1,
	FIPS_FATAL_HMAC_SHA256 = 1 << 2,
	FIPS_FATAL_HMAC_DRBG = 1 << 3,
	FIPS_FATAL_ECDSA = 1 << 4,
	FIPS_FATAL_AES128 = 1 << 6,
	FIPS_FATAL_CMAC_AES128 = 1 << 7,
	FIPS_ERROR_MASK = 0xff,
	FIPS_RFU_MASK = 0x7fffff00
};

extern uint32_t fips_status;
extern uint32_t fips_mode;

void _throw_fips_err(enum fips_err err, const char *func, int line);

/* update FIPS error status */
#define throw_fips_err(err) _throw_fips_err(err, __func__, __LINE__)

/**
 * Initialization
 * Single point of initialization for all FIPS-compliant
 * cryptography. Responsible for KATs, TRNG testing, and signalling a
 * fatal error.
 */
int init_fips(void);

void fips_init_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* __EC_BOARD_CR50_FIPS_H__ */
