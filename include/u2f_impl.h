/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* U2F implementation-specific callbacks and parameters. */

#ifndef __CROS_EC_U2F_IMPL_H
#define __CROS_EC_U2F_IMPL_H

#include "common.h"
#include "dcrypto.h"

/* ---- platform cryptography hooks ---- */

#define U2F_MAX_KH_SIZE 128 /* Max size of key handle */

/* ---- non-volatile U2F state, shared with common code ---- */
struct u2f_state {
	/* G2F key gen seed. */
	uint32_t salt[8];
	/* HMAC key for U2F key handle authentication. */
	uint32_t hmac_key[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	/* Stored DRBG entropy. */
	uint32_t drbg_entropy[16];
	size_t drbg_entropy_size;
};

/* Forward declarations to reduce dependencies. */
/* EC (uncompressed) point */
#define U2F_EC_KEY_SIZE	  P256_NBYTES /* EC key size in bytes */
#define U2F_EC_POINT_SIZE ((U2F_EC_KEY_SIZE * 2) + 1) /* Size of EC point */

#define U2F_POINT_UNCOMPRESSED 0x04 /* Uncompressed point format */

struct u2f_ec_point {
	uint8_t pointFormat; /* Point type */
	uint8_t x[U2F_EC_KEY_SIZE]; /* X-value */
	uint8_t y[U2F_EC_KEY_SIZE]; /* Y-value */
};

BUILD_ASSERT(sizeof(struct u2f_ec_point) == U2F_EC_POINT_SIZE);

struct u2f_signature {
	uint8_t sig_r[U2F_EC_KEY_SIZE]; /* Signature */
	uint8_t sig_s[U2F_EC_KEY_SIZE]; /* Signature */
};

/* Origin seed is a random nonce generated during key handle creation. */
#define U2F_ORIGIN_SEED_SIZE	    32
#define U2F_AUTHORIZATION_SALT_SIZE 16

#define U2F_V0_KH_SIZE 64

/* Key handle version = 0, only bound to device. */
struct u2f_key_handle_v0 {
	uint8_t origin_seed[U2F_ORIGIN_SEED_SIZE];
	uint8_t hmac[SHA256_DIGEST_SIZE];
};

BUILD_ASSERT(sizeof(struct u2f_key_handle_v0) <= U2F_MAX_KH_SIZE);
BUILD_ASSERT(sizeof(struct u2f_key_handle_v0) == U2F_V0_KH_SIZE);

/**
 * Key handle version = 1 for WebAuthn, bound to device and user.
 */
#define U2F_V1_KH_SIZE 113

/* Header is composed of version || origin_seed || kh_hmac */
#define U2F_V1_KH_HEADER_SIZE (U2F_ORIGIN_SEED_SIZE + SHA256_DIGEST_SIZE + 1)

struct u2f_key_handle_v1 {
	uint8_t version;
	uint8_t origin_seed[U2F_ORIGIN_SEED_SIZE];
	uint8_t kh_hmac[SHA256_DIGEST_SIZE];
	/* Optionally checked in u2f_sign. */
	uint8_t authorization_salt[U2F_AUTHORIZATION_SALT_SIZE];
	uint8_t authorization_hmac[SHA256_DIGEST_SIZE];
};

BUILD_ASSERT(sizeof(struct u2f_key_handle_v1) <= U2F_MAX_KH_SIZE);
BUILD_ASSERT(sizeof(struct u2f_key_handle_v1) == U2F_V1_KH_SIZE);


union u2f_key_handle_variant {
	struct u2f_key_handle_v0 v0;
	struct u2f_key_handle_v1 v1;
};

BUILD_ASSERT(sizeof(union u2f_key_handle_variant) <= U2F_MAX_KH_SIZE);

/**
 * Initialize state with newly generated keys.
 *
 * @param state U2F state to generate
 *
 * @return true if successful
 */
bool u2f_generate_state_keys(struct u2f_state *state);

/**
 * Update HMAC key in U2F state. Used when changing ownership to
 * cryptographically discard previously generated keys.
 */
bool u2f_generate_hmac_key(struct u2f_state *state);

/**
 * Create a randomized key handle for specified origin, user secret.
 * Generate associated signing key.
 *
 * @param state initialized u2f state
 * @param origin pointer to origin id
 * @param user pointer to user secret
 * @param authTimeSecretHash authentication time secret
 * @param kh output key handle header
 * @param kh_version - key handle version to generate
 * @param pubKey - generated public key
 *
 * @return true if a valid key pair was created
 */
bool u2f_generate(const struct u2f_state *state, const uint8_t *user,
		  const uint8_t *origin, const uint8_t *authTimeSecretHash,
		  union u2f_key_handle_variant *kh, uint8_t kh_version,
		  struct u2f_ec_point *pubKey);

/**
 * Create a randomized key handle for specified origin, user secret.
 * Generate associated signing key.
 *
 * @param state initialized u2f state
 * @param kh output key handle header
 * @param kh_version - key handle version to generate
 * @param origin pointer to origin id
 * @param user pointer to user secret
 * @param authTimeSecretHash pointer to user's authentication secret.
 *        can be set to NULL if authorization_hmac check is not needed.
 * @param r - generated part of signature
 * @param s - generated part of signature
 *
 * @return true if a valid key pair was created
 */
bool u2f_sign(const struct u2f_state *state,
	      const union u2f_key_handle_variant *kh, uint8_t kh_version,
	      const uint8_t *user, const uint8_t *origin,
	      const uint8_t *authTimeSecretHash, const uint8_t *hash,
	      struct u2f_signature *sig);

/**
 * Verify that key handle matches provided origin, user and user's
 * authentication secret and was created on this device (signed with
 * U2F state HMAC key).
 *
 * @param state initialized u2f state
 * @param kh input key handle
 * @param kh_version - key handle version to verify
 * @param user pointer to user secret
 * @param origin pointer to origin id
 * @param authTimeSecretHash pointer to user's authentication secret.
 *        can be set to NULL if authorization_hmac check is not needed.
 *
 * @return true if key handle was created on
 */
bool u2f_authorize_keyhandle(const struct u2f_state *state,
			     const union u2f_key_handle_variant *kh,
			     uint8_t kh_version, const uint8_t *user,
			     const uint8_t *origin,
			     const uint8_t *authTimeSecretHash);

/**
 * Gets the x509 certificate for the attestation key pair returned
 * by g2f_individual_keypair().
 *
 * @param state U2F state parameters
 * @param serial Device serial number
 * @param buf pointer to a buffer that must be at least
 *
 * G2F_ATTESTATION_CERT_MAX_LEN bytes.
 * @return size of certificate written to buf, 0 on error.
 */
int g2f_attestation_cert_serial(const struct u2f_state *state,
				const uint8_t *serial, uint8_t *buf);

/**
 * Verify that provided key handle and public key match.
 * @param state U2F state parameters
 * @param key_handle key handle
 * @param kh_version key handle version (0 - legacy, 1 - versioned)
 * @param user pointer to user secret
 * @param origin pointer to origin id
 * @param authTimeSecretHash pointer to user's authentication secret.
 *        can be set to NULL if authorization_hmac check is not needed.
 * @param public_key pointer to public key point (big endian)
 * @param data data to sign
 * @param data_size data size in bytes
 *
 * @param r part of generated signature
 * @param s part of generated signature
 *
 * @return true if public key matches key handle, (r,s) set to valid signature
 */
bool u2f_attest(const struct u2f_state *state,
		const union u2f_key_handle_variant *kh, uint8_t kh_version,
		const uint8_t *user, const uint8_t *origin,
		const uint8_t *authTimeSecretHash,
		const struct u2f_ec_point *public_key, const uint8_t *data,
		size_t data_size, struct u2f_signature *sig);

#endif /* __CROS_EC_U2F_IMPL_H */
