/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"

extern "C" {
#include "common.h"
#include "ec_commands.h"
#include "host_command.h"
#include "sha256.h"
#include "system.h"
#include "task.h"
#include "trng.h"
#include "util.h"
}

#include "fpsensor.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "fpsensor_utils.h"
#include "scoped_fast_cpu.h"

BUILD_ASSERT(FP_PAIRING_KEY_LEN == SHA256_DIGEST_SIZE);
BUILD_ASSERT(FP_PAIRING_KEY_EC_PUBLIC_KEY_LEN == 32);
BUILD_ASSERT(FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN == 32);

static enum ec_status
fp_command_establish_pairing_key_keygen(struct host_cmd_handler_args *args)
{
	auto *r = static_cast<ec_response_fp_establish_pairing_key_keygen *>(
		args->response);

	ScopedFastCpu fast_cpu;

	r->encrypted_private_key.info.struct_version =
		FP_PAIRING_KEY_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->encrypted_private_key.info.nonce,
			FP_PAIRING_KEY_NONCE_BYTES);
	trng_rand_bytes(r->encrypted_private_key.info.encryption_salt,
			FP_PAIRING_KEY_ENCRYPTION_SALT_BYTES);
	trng_exit();

	bssl::UniquePtr<EC_KEY> ecdh_key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	if (ecdh_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_KEY_generate_key(ecdh_key.get()) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_KEY_priv2oct(ecdh_key.get(), r->encrypted_private_key.data,
			    FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN) !=
	    FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	/* POINT_CONVERSION_UNCOMPRESSED indicates that the point is encoded as
	 * z||x||y, where z is the octet 0x04. */
	uint8_t *pubkey_ptr = nullptr;
	if (EC_KEY_key2buf(ecdh_key.get(), POINT_CONVERSION_UNCOMPRESSED,
			   &pubkey_ptr, nullptr) !=
	    FP_PAIRING_KEY_EC_PUBLIC_KEY_LEN * 2 + 1) {
		return EC_RES_UNAVAILABLE;
	}
	bssl::UniquePtr<uint8_t> pubkey_data(pubkey_ptr);
	memcpy(&r->pubkey, pubkey_data.get() + 1,
	       FP_PAIRING_KEY_EC_PUBLIC_KEY_LEN * 2);

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret = derive_encryption_key(
		key, r->encrypted_private_key.info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_keygen: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(
		key, SBP_ENC_KEY_LEN, r->encrypted_private_key.data,
		r->encrypted_private_key.data,
		FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN,
		r->encrypted_private_key.info.nonce, FP_PAIRING_KEY_NONCE_BYTES,
		r->encrypted_private_key.info.tag, FP_PAIRING_KEY_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_keygen: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PAIRING_KEY_KEYGEN,
		     fp_command_establish_pairing_key_keygen, EC_VER_MASK(0));

static enum ec_status
fp_command_establish_pairing_key_wrap(struct host_cmd_handler_args *args)
{
	const auto *params =
		static_cast<const ec_params_fp_establish_pairing_key_wrap *>(
			args->params);
	auto *r = static_cast<ec_response_fp_establish_pairing_key_wrap *>(
		args->response);

	ScopedFastCpu fast_cpu;

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret = derive_encryption_key(
		key, params->encrypted_private_key.info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	uint8_t privkey[FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN];

	memcpy(privkey, params->encrypted_private_key.data,
	       FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN);

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, privkey, privkey,
			      FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN,
			      params->encrypted_private_key.info.nonce,
			      FP_PAIRING_KEY_NONCE_BYTES,
			      params->encrypted_private_key.info.tag,
			      FP_PAIRING_KEY_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to decipher template");
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_KEY> ecdh_key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	if (ecdh_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_KEY_oct2priv(ecdh_key.get(), privkey,
			    FP_PAIRING_KEY_EC_PRIVATE_KEY_LEN) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	const EC_GROUP *group = EC_KEY_get0_group(ecdh_key.get());

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	/* POINT_CONVERSION_UNCOMPRESSED format point. */
	uint8_t public_point_buf[FP_PAIRING_KEY_EC_PUBLIC_KEY_LEN * 2 + 1];
	public_point_buf[0] = POINT_CONVERSION_UNCOMPRESSED;
	memcpy(public_point_buf + 1, &params->peers_pubkey,
	       FP_PAIRING_KEY_EC_PUBLIC_KEY_LEN * 2);

	if (EC_POINT_oct2point(group, public_point.get(), public_point_buf,
			       sizeof(public_point_buf), nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (ECDH_compute_key_fips(r->encrypted_pairing_key.data,
				  FP_PAIRING_KEY_LEN, public_point.get(),
				  ecdh_key.get()) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	r->encrypted_pairing_key.info.struct_version =
		FP_PAIRING_KEY_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->encrypted_pairing_key.info.nonce,
			FP_PAIRING_KEY_NONCE_BYTES);
	trng_rand_bytes(r->encrypted_pairing_key.info.encryption_salt,
			FP_PAIRING_KEY_ENCRYPTION_SALT_BYTES);
	trng_exit();

	ret = derive_encryption_key(
		key, r->encrypted_pairing_key.info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(
		key, SBP_ENC_KEY_LEN, r->encrypted_pairing_key.data,
		r->encrypted_pairing_key.data, FP_PAIRING_KEY_LEN,
		r->encrypted_pairing_key.info.nonce, FP_PAIRING_KEY_NONCE_BYTES,
		r->encrypted_pairing_key.info.tag, FP_PAIRING_KEY_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PAIRING_KEY_WRAP,
		     fp_command_establish_pairing_key_wrap, EC_VER_MASK(0));
