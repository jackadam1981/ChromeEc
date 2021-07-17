/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Helpers to emulate a U2F HID dongle over the TPM transport */

#include "console.h"
#include "dcrypto.h"
#include "extension.h"
#include "fips_rand.h"

#include "registers.h"
#include "signed_header.h"
#include "system.h"

#include "u2f.h"
#include "u2f_impl.h"
#include "util.h"

bool u2f_generate_hmac_key(struct u2f_state *state)
{
	/* HMAC key for key handle  */
	if (!fips_rand_bytes(state->salt_kek, sizeof(state->salt_kek)))
		return false;
	return true;
}

/* Generate U2F state keys in FIPS-compliant way */
bool u2f_generate_state_keys(struct u2f_state *state)
{
	/* G2F DRBG Entropy */
	if (!fips_rand_bytes(state->salt, sizeof(state->salt)))
		return false;

	if (!u2f_generate_hmac_key(state))
		return false;

	/* Get U2F entropy from health-checked TRNG */
	if (!fips_trng_bytes(state->salt_kh, sizeof(state->salt_kh)))
		return false;

	state->drbg_entropy_size = sizeof(state->salt_kh);
	return true;
}

/* Compute key handle HMAC */
static void u2f_origin_user_mac(const struct u2f_state *state,
				const uint8_t *user, const uint8_t *origin,
				const uint8_t *origin_seed, uint8_t kh_version,
				uint8_t *kh_hmac)
{
	LITE_HMAC_CTX ctx;

	/* HMAC(u2f_hmac_key, origin || user || origin seed || version) */

	DCRYPTO_HMAC_SHA256_init(&ctx, state->salt_kek, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, origin, P256_NBYTES);
	HASH_update(&ctx.hash, user, P256_NBYTES);
	HASH_update(&ctx.hash, origin_seed, P256_NBYTES);
	if (kh_version != 0)
		HASH_update(&ctx.hash, &kh_version, sizeof(kh_version));

	memcpy(kh_hmac, DCRYPTO_HMAC_final(&ctx), SHA256_DIGEST_SIZE);
}

static void u2f_authorization_mac(const struct u2f_state *state,
				  union u2f_key_handle_variant *kh,
				  const uint8_t *auth_time_secret_hash)
{
	LITE_HMAC_CTX ctx;

	/**
	 * HMAC(u2f_hmac_key, auth_salt || (key_handle_header)
	 *                              || authTimeSecret)
	 */
	DCRYPTO_HMAC_SHA256_init(&ctx, state->salt_kek, SHA256_DIGEST_SIZE);
	HASH_update(&ctx.hash, kh->vkh.authorization_salt,
		    U2F_AUTHORIZATION_SALT_SIZE);
	HASH_update(&ctx.hash, (uint8_t *)&kh->vkh.header,
		    sizeof(kh->vkh.header));

	HASH_update(&ctx.hash, auth_time_secret_hash, SHA256_DIGEST_SIZE);

	memcpy(kh->vkh.authorization_hmac, DCRYPTO_HMAC_final(&ctx),
	       SHA256_DIGEST_SIZE);
}

static int app_hw_device_id(enum dcrypto_appid appid, const uint32_t input[8],
			    uint32_t output[8])
{
	struct APPKEY_CTX ctx;
	int result;

	/**
	 * Setup USR-based application key. This loads (if not already done)
	 * application-specific DeviceID.
	 * Internally it computes:
	 * HMAC(hw_device_id, SHA256(name[appid])), but we don't care about
	 * process.
	 * Important property:
	 *          For same appid it will load same value.
	 */
	if (!DCRYPTO_appkey_init(appid, &ctx))
		return 0;

	/**
	 * Compute HMAC(HMAC(hw_device_id, SHA256(name[appid])), input)
	 * It is not used as a key though, and treated as personalization
	 * string for DRBG.
	 */
	result = DCRYPTO_appkey_derive(appid, input, output);

	DCRYPTO_appkey_finish(&ctx);
	return result;
}

/**
 * Generate an origin and user-specific ECDSA keypair from the specified
 * key handle.
 *
 * If pk_x and pk_y are NULL, public key generation will be skipped.
 *
 * @param state U2F state parameters
 * @param kh key handle
 * @param kh_version key handle version (0 - legacy, 1 - versioned)
 * @param d pointer to ECDSA private key
 * @param pk_x pointer to public key point
 * @param pk_y pointer to public key point
 *
 * @return EC_SUCCESS if a valid keypair was created.
 */
static int u2f_origin_user_keypair(const struct u2f_state *state,
				   const union u2f_key_handle_variant *kh,
				   uint8_t kh_version, p256_int *d,
				   p256_int *pk_x, p256_int *pk_y)
{
	uint32_t dev_salt[P256_NDIGITS];
	uint8_t key_seed[P256_NBYTES];

	struct drbg_ctx drbg;
	size_t key_handle_size;
	uint8_t *key_handle;

	if (kh_version == 0) {
		key_handle_size = sizeof(struct u2f_key_handle);
		key_handle = (uint8_t *)&kh->kh;
	} else {
		key_handle_size =
			sizeof(struct u2f_versioned_key_handle_header);
		key_handle = (uint8_t *)&kh->vkh.header;
	}

	if (!app_hw_device_id(U2F_ORIGIN, state->salt_kek, dev_salt))
		return EC_ERROR_UNKNOWN;

	hmac_drbg_init(&drbg, state->salt_kh, P256_NBYTES, dev_salt,
		       P256_NBYTES, NULL, 0);

	hmac_drbg_generate(&drbg, key_seed, sizeof(key_seed), key_handle,
			   key_handle_size);

	if (!DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, key_seed))
		return EC_ERROR_TRY_AGAIN;

	return EC_SUCCESS;
}

/**
 * Create a randomized key handle using random origin seed for
 * specified origin, user secret. Generate associated signing
 * key.
 *
 * @param state initialized u2f state
 * @param origin pointer to origin id
 * @param user pointer to user secret
 * @param authTimeSecretHash authentication time secret
 * @param kh output key handle header
 * @param kh_version - key handle version to generate
 * @param od generated private key
 * @param pubKey - generated public key
 *
 * @return true if a valid keypair was created
 */
bool u2f_generate(const struct u2f_state *state, const uint8_t *user,
		  const uint8_t *origin, const uint8_t *authTimeSecretHash,
		  union u2f_key_handle_variant *kh, uint8_t kh_version,
		  struct u2f_ec_point *pubKey)
{
	uint8_t *kh_hmac, *kh_origin_seed;
	int generate_keypair_rc;
	/* Generated public keys associated with key handle */
	p256_int opk_x, opk_y;

	/* Compute constants for request key handler version */
	if (kh_version == 0) {
		kh_hmac = kh->kh.hmac;
		kh_origin_seed = kh->kh.origin_seed;
	} else {
		kh_hmac = kh->vkh.header.kh_hmac;
		kh_origin_seed = kh->vkh.header.origin_seed;
		kh->vkh.header.version = kh_version;
		if (!fips_rand_bytes(kh->vkh.authorization_salt,
				     U2F_AUTHORIZATION_SALT_SIZE))
			return false;
	}

	/* Generate key handle header candidates and origin-specific keypair */
	do {
		p256_int od;
		/* Generate random origin seed for key handle candidate */
		if (!fips_rand_bytes(kh_origin_seed, U2F_ORIGIN_SEED_SIZE))
			return false;

		u2f_origin_user_mac(state, user, origin, kh_origin_seed,
				    kh_version, kh_hmac);

		/**
		 * Try to generate key pair using key handle. This may fail if
		 * key handle results in private key which is out of allowed
		 * range. If this is the case, repeat with another origin seed.
		 */
		generate_keypair_rc = u2f_origin_user_keypair(
			state, kh, kh_version, &od, &opk_x, &opk_y);
	} while (generate_keypair_rc == EC_ERROR_TRY_AGAIN);

	if (generate_keypair_rc != EC_SUCCESS)
		return false;

	if (kh_version != 0)
		u2f_authorization_mac(state, kh, authTimeSecretHash);

	pubKey->pointFormat = U2F_POINT_UNCOMPRESSED;
	p256_to_bin(&opk_x, pubKey->x); /* endianness */
	p256_to_bin(&opk_y, pubKey->y); /* endianness */

	return true;
}

bool u2f_authorize_keyhandle(const struct u2f_state *state,
			     const union u2f_key_handle_variant *kh,
			     uint8_t kh_version, const uint8_t *user,
			     const uint8_t *origin)
{
	/* Re-created key handle. */
	uint8_t recreated_kh_hmac[SHA256_DIGEST_SIZE];
	const uint8_t *origin_seed, *kh_hmac;

	/*
	 * Re-create the key handle and compare against that which
	 * was provided. This allows us to verify that the key handle
	 * is owned by this combination of device, current user and origin.
	 */
	if (kh_version == 0) {
		origin_seed = kh->kh.origin_seed;
		kh_hmac = kh->kh.hmac;
	} else {
		origin_seed = kh->vkh.header.origin_seed;
		kh_hmac = kh->vkh.header.kh_hmac;
	}

	u2f_origin_user_mac(state, user, origin, origin_seed, kh_version,
			    recreated_kh_hmac);

	return safe_memcmp(&recreated_kh_hmac, kh_hmac,
			   sizeof(recreated_kh_hmac)) == 0;
}

static bool
u2f_attest_keyhanlde_pubkey(const struct u2f_state *state,
			    const union u2f_key_handle_variant *key_handle,
			    uint8_t kh_version, const uint8_t *user,
			    const uint8_t *origin,
			    const struct u2f_ec_point *public_key)
{
	struct u2f_ec_point kh_pubkey;
	p256_int od, opk_x, opk_y;

	/* Check this is a correct key handle for provided user/origin */
	if (!u2f_authorize_keyhandle(state, key_handle, kh_version, user,
				     origin))
		return false;

	/* Recreate public key from key handle */
	if (u2f_origin_user_keypair(state, key_handle, kh_version, &od, &opk_x,
				    &opk_y) != EC_SUCCESS)
		return false;
	p256_clear(&od);
	/* Reconstruct the public key. */
	p256_to_bin(&opk_x, kh_pubkey.x);
	p256_to_bin(&opk_y, kh_pubkey.y);
	kh_pubkey.pointFormat = U2F_POINT_UNCOMPRESSED;

	return safe_memcmp(&kh_pubkey, public_key,
			   sizeof(struct u2f_ec_point)) == 0;
}

bool u2f_sign(const struct u2f_state *state,
	      const union u2f_key_handle_variant *kh, uint8_t kh_version,
	      const uint8_t *user, const uint8_t *origin, const uint8_t *hash,
	      struct u2f_signature *sig)
{
	/* Origin private key. */
	p256_int origin_d;

	/* Hash, and corresponding signature. */
	p256_int h, r, s;

	struct drbg_ctx ctx;

	bool result = false;

	if (!u2f_authorize_keyhandle(state, kh, kh_version, user, origin))
		return false;

	/* Re-create origin-specific key. */
	if (!u2f_origin_user_keypair(state, kh, kh_version, &origin_d, NULL,
				     NULL) != EC_SUCCESS)
		return false;

	/* Prepare hash to sign. */
	p256_from_bin(hash, &h);

	/* Sign. */
	hmac_drbg_init_rfc6979(&ctx, &origin_d, &h);
	result = dcrypto_p256_ecdsa_sign(&ctx, &origin_d, &h, &r, &s) != 0;

	p256_clear(&origin_d);

	p256_to_bin(&r, sig->sig_r);
	p256_to_bin(&s, sig->sig_s);

	return result;
}

/**
 * Generate a hardware derived ECDSA keypair for individual attestation.
 *
 * @param state U2F state parameters
 * @param d pointer to ECDSA private key
 * @param pk_x pointer to public key point
 * @param pk_y pointer to public key point
 *
 * @return EC_SUCCESS if a valid keypair was created.
 */
static bool g2f_individual_keypair(const struct u2f_state *state, p256_int *d,
				   p256_int *pk_x, p256_int *pk_y)
{
	uint8_t buf[SHA256_DIGEST_SIZE];

	/* Incorporate HIK & diversification constant */
	if (!app_hw_device_id(U2F_ATTEST, state->salt, (uint32_t *)buf))
		return false;

	/* Generate unbiased private key */
	while (!DCRYPTO_p256_key_from_bytes(pk_x, pk_y, d, buf)) {
		HASH_CTX sha;

		DCRYPTO_SHA256_init(&sha, 0);
		HASH_update(&sha, buf, sizeof(buf));
		memcpy(buf, HASH_final(&sha), sizeof(buf));
	}

	return true;
}

#define G2F_CERT_NAME "CrO2"

int g2f_attestation_cert_serial(const struct u2f_state *state,
				const uint8_t *serial, uint8_t *buf)
{
	p256_int d, pk_x, pk_y;

	if (g2f_individual_keypair(state, &d, &pk_x, &pk_y))
		return 0;

	/* Note that max length is not currently respected here. */
	return DCRYPTO_x509_gen_u2f_cert_name(&d, &pk_x, &pk_y,
					      (p256_int *)serial, G2F_CERT_NAME,
					      buf,
					      G2F_ATTESTATION_CERT_MAX_LEN);
}

bool u2f_attest(const struct u2f_state *state,
		const union u2f_key_handle_variant *kh, uint8_t kh_version,
		const uint8_t *user, const uint8_t *origin,
		const struct u2f_ec_point *public_key, const uint8_t *data,
		size_t data_size, struct u2f_signature *sig)
{
	struct HASH_CTX h_ctx;
	struct drbg_ctx dr_ctx;

	/* Data hash, and corresponding signature. */
	p256_int h, r, s;

	/* Attestation key */
	p256_int d, pk_x, pk_y;

	int result;

	if (!u2f_attest_keyhanlde_pubkey(state, kh, kh_version, user, origin,
					 public_key))
		return false;

	/* Message signature */
	DCRYPTO_SHA256_init(&h_ctx, 0);
	HASH_update(&h_ctx, data, data_size);
	p256_from_bin(HASH_final(&h_ctx), &h);

	/* Derive G2F Attestation Key */
	if (g2f_individual_keypair(state, &d, &pk_x, &pk_y)) {
		/* CPRINTF("G2F Attestation key generation failed"); */
		return false;
	}

	/* Sign over the response w/ the attestation key */
	hmac_drbg_init_rfc6979(&dr_ctx, &d, &h);

	result = dcrypto_p256_ecdsa_sign(&dr_ctx, &d, &h, &r, &s);
	p256_clear(&d);

	p256_to_bin(&r, sig->sig_r);
	p256_to_bin(&s, sig->sig_s);

	return result != 0;
}
