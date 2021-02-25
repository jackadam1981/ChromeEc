/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Includes */
#include "dcrypto.h"
#include "lms.h"
#include "console.h"
#include "time.h"
#include "watchdog.h"
#include "registers.h"

/**
 * HSS Signature Verification code
 *
 * Configuration variables:
 * SAVE_RAM_WITH_SW_SHA - in lm_ots_validate_signature_compute use software
 * SHA256 implementation to save RAM for OTS signatures hash if not defined, use
 * RAM buffer either in RAM or on stack to store intermediate hashes as hardware
 * SHA engine only supports single context
 *
 * This is the file that implements the hashing APIs we use internally.
 * At the present, our parameter sets support only one hash function
 * (SHA-256, using full 256 bit output), however, that is likely to change
 * in the future
 *
 * This version use xx_sha_256_2 to compute SHA256(input1 || input2) in
 * a single shot, which is faster than livestream mode (18ms vs. 23ms)
 */


union hash_context {
        HASH_CTX sha256;
        /* Any other hash contexts would go here */
};

/* One shot processing of sha-256 to assist PQ computations */
// inline doesn't really help
void xx_sha256_2(const void *input1, size_t size1, const void *input2,
                        size_t size2, uint32_t out[], size_t n)
{
        /* HASH_CTX ctx; */
	int i;
	const uint8_t *input;
        /**
         * TODO: On Dauntless this can be one-shot operation
         * reg_hmac->cfg = 0; // not a live stream
         * reg_hmac->msglen[0] = size1 + size2;
         * reg_hmac->msglen[1] = 0;
         * reg_hmac->trig = HMAC_SHA_TRIG_GO_MASK;
         * xx_hmac_send_input(input1, size1);
         * xx_hmac_send_input(input2, size2);
         * xx_sha256_read(out);
         */

	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, LIVESTREAM, 0);
	GREG32(KEYMGR, SHA_CFG_MSGLEN_LO) = size1 + size2;
	GREG32(KEYMGR, SHA_CFG_MSGLEN_HI) = 0;
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_GO, 1);

	input = input1;
	for (i = 0; i < size1; ++i, ++input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *input;
	}
	input = input2;
	for (i = 0; i < size2; ++i, ++input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *input;
	}

	/* We don't need to TRIG_STOP since not livestream */

	while (!GREG32(KEYMGR, SHA_ITOP)) {
		continue;
	}
        /* Clear status. */
	GREG32(KEYMGR, SHA_ITOP) = 0;

	// Only pull out 6 since N = 24 (not 32)
	out[0] = GREG32(KEYMGR, SHA_STS_H0);
	out[1] = GREG32(KEYMGR, SHA_STS_H1);
	out[2] = GREG32(KEYMGR, SHA_STS_H2);
	out[3] = GREG32(KEYMGR, SHA_STS_H3);
	out[4] = GREG32(KEYMGR, SHA_STS_H4);
	out[5] = GREG32(KEYMGR, SHA_STS_H5);
}

void xx_sha256_3(const void *input1, size_t size1, const void *input2,
                        size_t size2, const void *input3,
                        size_t size3, uint32_t out[], size_t n)
{
        /* HASH_CTX ctx; */
	int i;
	const uint8_t *input;

	GWRITE_FIELD(KEYMGR, SHA_CFG_EN, LIVESTREAM, 0);
	GREG32(KEYMGR, SHA_CFG_MSGLEN_LO) = size1 + size2 + size3;
	GREG32(KEYMGR, SHA_CFG_MSGLEN_HI) = 0;
	GWRITE_FIELD(KEYMGR, SHA_TRIG, TRIG_GO, 1);

	input = input1;
	for (i = 0; i < size1; ++i, ++input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *input;
	}
	input = input2;
	for (i = 0; i < size2; ++i, ++input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *input;
	}
	input = input3;
	for (i = 0; i < size2; ++i, ++input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = *input;
	}

	/* We don't need to TRIG_STOP since not livestream */

	while (!GREG32(KEYMGR, SHA_ITOP)) {
		continue;
	}
        /* Clear status. */
	GREG32(KEYMGR, SHA_ITOP) = 0;

	// Only pull out 6 since N = 24 (not 32)
	out[0] = GREG32(KEYMGR, SHA_STS_H0);
	out[1] = GREG32(KEYMGR, SHA_STS_H1);
	out[2] = GREG32(KEYMGR, SHA_STS_H2);
	out[3] = GREG32(KEYMGR, SHA_STS_H3);
	out[4] = GREG32(KEYMGR, SHA_STS_H4);
	out[5] = GREG32(KEYMGR, SHA_STS_H5);
}

/*
 * Internal utility to convert encoded parameter sets into what they represent
 */
static const struct lms_params lms_params[] =
        {{LMS_SHA256_M32_H20, 20, 32}, {LMS_SHA256_M24_H20, 20, 24},
         {LMS_SHA256_M32_H15, 15, 32}, {LMS_SHA256_M24_H15, 15, 24},
         {LMS_SHA256_M32_H25, 25, 32}, {LMS_SHA256_M24_H25, 25, 24},
         {LMS_SHA256_M32_H10, 10, 32}, {LMS_SHA256_M24_H10, 10, 24},
         {LMS_SHA256_M32_H5, 5, 32},   {LMS_SHA256_M24_H5, 5, 24}};

/* return height of the tree, or zero if parameter set is invalid */
const struct lms_params *lm_look_up_parameter_set(
        enum lms_algorithm_type type)
{
        for (size_t i = 0; i < ARRAY_SIZE(lms_params); i++)
                if (lms_params[i].type == type)
                        return &lms_params[i];
        return NULL;
}

/**
 *   u = ceil(8*n/w)
 *   v = ceil((floor(log2((2^w - 1) * u)) + 1) / w)
 *   p = u + v
 *   ls = 16 - (v * w)
 *   max_digit = (1 << w) - 1;
 */
static const struct lmots_params lmots_params[] = {
        {LMOTS_SHA256_N32_W8, 3, 32, 34, 0, 255},
        {LMOTS_SHA256_N24_W8, 3, 24, 26, 0, 255},
        {LMOTS_SHA256_N32_W4, 2, 32, 67, 4, 15},
        {LMOTS_SHA256_N24_W4, 2, 24, 51, 4, 15},
        {LMOTS_SHA256_N32_W2, 1, 32, 133, 6, 3},
        {LMOTS_SHA256_N24_W2, 1, 24, 101, 6, 3},
        {LMOTS_SHA256_N32_W1, 0, 32, 265, 7, 1},
        {LMOTS_SHA256_N24_W1, 0, 24, 200, 8, 1},
};

/*
 * Convert the external name of a parameter set into the set of values we care
 * about
 */
const struct lmots_params *lm_ots_look_up_parameter_set(
        enum lmots_algorithm_type type)
{
        for (size_t i = 0; i < ARRAY_SIZE(lmots_params); i++)
                if (lmots_params[i].type == type)
                        return &lmots_params[i];
        return NULL;
}

static uint32_t lm_ots_coef(const void *Q, uint32_t i,
                            const struct lmots_params *lmots)
{
        /* Which byte holds the coefficient we want */
        uint32_t index = (i << lmots->logw) / 8;
        uint32_t digits_per_byte = 8 >> lmots->logw;
        /* Where in the byte the coefficient is */
        uint32_t shift = (~i & (digits_per_byte - 1)) << lmots->logw;
        uint8_t *q = (uint8_t *)Q;

        return (q[index] >> shift) & lmots->max_digit;
}

/* Compute Winternitz checksum to append to the hash */
uint16_t lm_ots_compute_checksum(const void *Q,
                                        const struct lmots_params *lmots)
{
        uint32_t sum = 0;
        uint32_t i;
        uint32_t u = (8 * lmots->n) >> lmots->logw;
        for (i = 0; i < u; i++) {
                sum += lmots->max_digit - lm_ots_coef(Q, i, lmots);
        }
        return htobe16(sum << lmots->ls);
}

void lm_ots_compute_pub_key(const struct lmots_params *lmots, const ilen_t I,
			    merkle_index_t q, const uint8_t private_data[],
			    struct Sha *pub_key_out)
{
        /* 4-byte align to more efficient SHA read */
        struct Sha hashes[26];
        uint8_t iterations[26];
        struct ots_public_key hash_buf;

        memcpy(hashes, private_data, sizeof(hashes));
        memset(iterations, 255, 26);

        lm_ots_compute_pub_hash(lmots, I, q, hashes, iterations);

        /**
         * K = H(I || u32str(q) || u16str(D_PBLC) || y[0] || ... || y[p-1])
         */
        /* Hashing the OTS public key */
        memcpy(hash_buf.I, I, I_LEN);
	hash_buf.q = htobe32(q);
	hash_buf.d = D_PBLC;
	xx_sha256_2(&hash_buf, PBLC_PREFIX_LEN, hashes, sizeof(hashes),
		    pub_key_out->value, lmots->n);
}

void lm_ots_compute_sig(const struct lmots_params *lmots, const ilen_t I,
			merkle_index_t q, const uint8_t private_data[],
			const struct Sha* message_digest,
			struct LeafSig *leaf_sig_out)
{
	/* 4-byte align to more efficient SHA read */
	uint8_t iterations[26];
        /* message with checksum */
        uint32_t Q[SHA256_DIGEST_WORDS + 1];
        size_t i;


        memcpy(Q, message_digest->value, lmots->n);
        /* Append the checksum to the hash, assume n is divisible by 4 */
        Q[lmots->n /4] = lm_ots_compute_checksum(Q, lmots);

        for (i = 0; i < sizeof(iterations); ++i) {
                iterations[i] = lm_ots_coef(Q, i, lmots);
        }

        memcpy(leaf_sig_out->hashes, private_data, 26 * 24 / 4 /* TODO change to member_size */);

        lm_ots_compute_pub_hash(lmots, I, q, leaf_sig_out->hashes, iterations);
}


void lm_ots_compute_pub_hash(
        const struct lmots_params *lmots, const ilen_t I, merkle_index_t q,
        struct Sha in_out_data[], const uint8_t num_hash[])
{

        const uint32_t n = lmots->n;
        const uint32_t p = lmots->p;
	const uint32_t max_digit = lmots->max_digit;

        /* The Winternitz iteration hashes */
        /* Preset the parts of tmp that don't change */
        struct wots tx;
        memcpy(tx.I, I, I_LEN);
        tx.q = htobe32(q);

        /**
         * This loop computes on average 34*128 SHA256 of 55 byte blocks
         */
        for (size_t i = 0; i < p; i++) {
                tx.k = htobe16(i);
                for (uint32_t j = max_digit - num_hash[i]; j < max_digit; j++) {
                        tx.j = j;
			/* potential opportunity for HW acceleration */
			xx_sha256_2(&tx.I, ITER_PREV, in_out_data[i].value, n,
				    in_out_data[i].value, n);
		}
	}
}
