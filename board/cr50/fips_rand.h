/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_BOARD_CR50_FIPS_RAND_H
#define __EC_BOARD_CR50_FIPS_RAND_H

#include <stddef.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "dcrypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * TRNG health tests
 * For H1 minimal assessed entropy H >=0.85 for 1-bit samples
 * using NIST Entropy Assessment tool.
 * If any of the approved continuous health tests are used by the entropy
 * source, the false positive probability for these tests shall be set to
 * at least 2^(-50)  (NIST SP 800-90B 4.3).
 * Reason for 2^(-50) vs 2^(-40) is to minimize impact to user experience
 * due to false positives.
 */

/**
 * The entropy source's startup tests shall run the continuous health tests
 * over at least 4096 consecutive samples.
 */
#define TRNG_INIT_BITS (4096 * TRNG_SAMPLE_BITS)
#define TRNG_INIT_WORDS (TRNG_INIT_BITS / 32)

/**
 * (1) Repetition count test NIST SP 800-90B 4.4.1
 * Cut off value is computed as:
 * c = ceil(1 + (-log2 alpha)/H);
 * alpha = 2^-50, H = 0.85
 */
#if TRNG_SAMPLE_BITS == 1
#define RCT_CUTOFF_SAMPLES 59
#elif TRNG_SAMPLE_BITS == 2
#define RCT_CUTOFF_SAMPLES 30
#elif TRNG_SAMPLE_BITS == 3
#define RCT_CUTOFF_SAMPLES 20
#elif TRNG_SAMPLE_BITS == 4
#define RCT_CUTOFF_SAMPLES 15
#elif TRNG_SAMPLE_BITS == 5
#define RCT_CUTOFF_SAMPLES 12
#elif TRNG_SAMPLE_BITS == 6
#define RCT_CUTOFF_SAMPLES 10
#elif TRNG_SAMPLE_BITS == 7
#define RCT_CUTOFF_SAMPLES 9
#elif TRNG_SAMPLE_BITS == 8
#define RCT_CUTOFF_SAMPLES 8
#endif
#define RCT_CUTOFF_WORDS ((RCT_CUTOFF_SAMPLES * TRNG_SAMPLE_BITS + 31) / 32)

int rct_is_initialized(void);
void repetition_count_test(uint32_t val);

/**
 * (2) Adaptive Proportion Test, NIST SP 800-90B 4.4.2, Table 2
 */
#if TRNG_SAMPLE_BITS == 1
/* APT Windows size W = 1024 for 1 bit samples */
#define APT_WINDOW_SIZE_SAMPLES 1024
#else
/* or 512 samples if more than 1 bit per sample */
#define APT_WINDOW_SIZE_SAMPLES 512
#endif

#define APT_WINDOW_SIZE_BITS (APT_WINDOW_SIZE_SAMPLES * TRNG_SAMPLE_BITS)
#define APT_WINDOW_SIZE_NWORDS ((APT_WINDOW_SIZE_BITS + 31) / 32)

/**
 * Cut off value =CRITBINOM(W, power(2,(-H)),1-α).
 * 692 =CRITBINOM(1024, power(2,(-0.85)), 1 - 2^(-50))
 */
#if TRNG_SAMPLE_BITS == 1
#define APT_CUTOFF_SAMPLES 692
#elif TRNG_SAMPLE_BITS == 2
#define APT_CUTOFF_SAMPLES 239
#elif TRNG_SAMPLE_BITS == 3
#define APT_CUTOFF_SAMPLES 158
#elif TRNG_SAMPLE_BITS == 4
#define APT_CUTOFF_SAMPLES 106
#elif TRNG_SAMPLE_BITS == 5
#define APT_CUTOFF_SAMPLES 72
#elif TRNG_SAMPLE_BITS == 6
#define APT_CUTOFF_SAMPLES 52
#elif TRNG_SAMPLE_BITS == 7
#define APT_CUTOFF_SAMPLES 37
#elif TRNG_SAMPLE_BITS == 8
#define APT_CUTOFF_SAMPLES 28
#endif

/**
 * FIPS-compliant TRNG startup.
 * The entropy source's startup tests shall run the continuous health tests
 * over at least 4096 consecutive samples.
 * Note: This function can set the global 'fips_error' variable.
 */
void fips_trng_startup(void);

void fips_trng_bytes(void *buffer, size_t len);

/**
 * store full (condensed) entropy into buffer.
 */
void get_entropy(void *buffer, size_t len);

/**
 * get 32 bits of full (condensed) entropy.
 */
uint32_t get_entropy32(void);

/* initialize cr50-wide DRBG replacing rand */
int fips_drbg_init(void);

/* mark cr50-wide DRBG as not initialized */
void fips_drbg_init_clear(void);

/* random bytes using FIPS-compliant HMAC_DRBG */
int fips_rand_bytes(void *buffer, size_t len);

/* wrapper around dcrypto_p256_ecdsa_sign using FIPS-compliant HMAC_DRBG */
int fips_p256_ecdsa_sign(const p256_int *key, const p256_int *message,
			 p256_int *r, p256_int *s);

/**
 * wrapper around hmac_drbg_generate to automatically reseed drbg
 * when needed.
 */
enum hmac_result fips_hmac_drbg_generate_reseed(struct drbg_ctx *ctx, void *out,
						size_t out_len,
						const void *input,
						size_t input_len);

#ifdef __cplusplus
}
#endif

#endif /* ! __EC_BOARD_CR50_FIPS_RAND_H */
