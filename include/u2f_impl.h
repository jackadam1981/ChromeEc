/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* U2F implementation-specific callbacks and parameters. */

#ifndef __CROS_EC_U2F_IMPL_H
#define __CROS_EC_U2F_IMPL_H

#include "common.h"

#ifdef TEST_BUILD
#include "board/host/dcrypto.h"
#endif

#define SHA256_DIGEST_SIZE 32

#include "tpm_vendor_cmds.h"

/* ---- platform cryptography hooks ---- */

/* ---- non-volatile U2F state, shared with common code ---- */
struct u2f_state {
	uint32_t salt[8]; /* G2F DRBG entropy */
	/* HMAC key for key handle */
	uint32_t salt_kek[SHA256_DIGEST_SIZE / sizeof(uint32_t)];
	uint32_t salt_kh[16]; /* DRBG entropy, 512 bits */
	uint32_t drbg_entropy_size;
};

/* Forward declarations to reduce dependencies. */
struct u2f_ec_point;
struct u2f_signature;

#define U2F_KH_VERSION_1	    0x01
#define U2F_ORIGIN_SEED_SIZE	    32
#define U2F_AUTHORIZATION_SALT_SIZE 16
#define U2F_MAX_KH_SIZE		    128 /* Max size of key handle */

struct u2f_key_handle {
	uint8_t origin_seed[U2F_ORIGIN_SEED_SIZE];
	uint8_t hmac[SHA256_DIGEST_SIZE];
};

struct u2f_versioned_key_handle_header {
	uint8_t version;
	uint8_t origin_seed[U2F_ORIGIN_SEED_SIZE];
	uint8_t kh_hmac[SHA256_DIGEST_SIZE];
};

struct u2f_versioned_key_handle {
	struct u2f_versioned_key_handle_header header;
	/* Optionally checked in u2f_sign. */
	uint8_t authorization_salt[U2F_AUTHORIZATION_SALT_SIZE];
	uint8_t authorization_hmac[SHA256_DIGEST_SIZE];
};

union u2f_key_handle_variant {
	struct u2f_key_handle kh;
	struct u2f_versioned_key_handle vkh;
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
 * @return true if a valid keypair was created
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
 * @param r - generated part of signature
 * @param s - generated part of signature
 *
 * @return true if a valid keypair was created
 */
bool u2f_sign(const struct u2f_state *state,
	      const union u2f_key_handle_variant *kh, uint8_t kh_version,
	      const uint8_t *user, const uint8_t *origin, const uint8_t *hash,
	      struct u2f_signature *sig);

/**
 * Verify that key handle matches provided origin and user and was created
 * on this device (signed with U2F state HMAC key).
 *
 * @param state initialized u2f state
 * @param kh input key handle
 * @param kh_version - key handle version to verify
 * @param user pointer to user secret
 * @param origin pointer to origin id
 *
 * @return true if key handle was created on
 */
bool u2f_authorize_keyhandle(const struct u2f_state *state,
			     const union u2f_key_handle_variant *kh,
			     uint8_t kh_version, const uint8_t *user,
			     const uint8_t *origin);

/* Maximum size in bytes of G2F attestation certificate. */
#define G2F_ATTESTATION_CERT_MAX_LEN 315

/**
 * Gets the x509 certificate for the attestation keypair returned
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
 * @param public_key pointer to public key point (big endian)
 * @param data data to sign
 * @param data_size data size in bytes
 * @param r part of generated signature
 * @param s part of generated signature
 *
 * @return true if public key matches key handle.
 */
bool u2f_attest(const struct u2f_state *state,
		const union u2f_key_handle_variant *kh, uint8_t kh_version,
		const uint8_t *user, const uint8_t *origin,
		const struct u2f_ec_point *public_key, const uint8_t *data,
		size_t data_size, struct u2f_signature *sig);

#endif /* __CROS_EC_U2F_IMPL_H */
