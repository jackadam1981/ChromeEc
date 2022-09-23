/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "openssl/bn.h"
#include "openssl/ec.h"
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
#include "fpsensor_auth_commands.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "fpsensor_utils.h"
#include "scoped_fast_cpu.h"

#include <algorithm>
#include <array>

BUILD_ASSERT(FP_PK_LEN == SHA256_DIGEST_SIZE);
BUILD_ASSERT(FP_PK_EC_PUBLIC_KEY_LEN == 32);
BUILD_ASSERT(FP_PK_EC_PRIVATE_KEY_LEN == 32);

/* The GSC paring key. */
static std::array<uint8_t, FP_PK_LEN> pairing_key;

/**
 * @warning |fp_buffer| contains data used by the matching algorithm that must
 * be released by calling fp_sensor_deinit() first. Call
 * fp_reset_and_clear_context instead of calling this directly.
 */
void fp_clear_context(void)
{
	templ_valid = 0;
	templ_dirty = 0;
	OPENSSL_cleanse(fp_buffer, sizeof(fp_buffer));
	OPENSSL_cleanse(fp_enc_buffer, sizeof(fp_enc_buffer));
	OPENSSL_cleanse(user_id, sizeof(user_id));
	fp_disable_positive_match_secret(&positive_match_secret_state);
	for (uint16_t idx = 0; idx < FP_MAX_FINGER_COUNT; idx++)
		fp_clear_finger_context(idx);
}

static enum ec_status
fp_command_establish_pk_keygen(struct host_cmd_handler_args *args)
{
	auto *r = static_cast<ec_response_fp_establish_pk_keygen *>(
		args->response);

	ScopedFastCpu fast_cpu;

	r->enc_privkey_info.struct_version = FP_PK_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN);
	trng_rand_bytes(r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES);
	trng_rand_bytes(r->enc_privkey_info.encryption_salt,
			FP_PK_ENCRYPTION_SALT_BYTES);
	trng_exit();

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> secret(
		BN_bin2bn(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (secret == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), secret.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(group.get(), public_point.get(),
						x_bn.get(), y_bn.get(),
						nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(x_bn.get(), r->pubkey_x, FP_PK_EC_PUBLIC_KEY_LEN) !=
	    FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(y_bn.get(), r->pubkey_y, FP_PK_EC_PUBLIC_KEY_LEN) !=
	    FP_PK_EC_PUBLIC_KEY_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret =
		derive_encryption_key(key, r->enc_privkey_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(key, SBP_ENC_KEY_LEN, r->enc_privkey,
			      r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN,
			      r->enc_privkey_info.nonce, FP_PK_NONCE_BYTES,
			      r->enc_privkey_info.tag, FP_PK_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_keygen: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PK_KEYGEN,
		     fp_command_establish_pk_keygen, EC_VER_MASK(0));

static enum ec_status
fp_command_establish_pk_wrap(struct host_cmd_handler_args *args)
{
	const auto *params =
		static_cast<const ec_params_fp_establish_pk_wrap *>(
			args->params);
	auto *r =
		static_cast<ec_response_fp_establish_pk_wrap *>(args->response);

	ScopedFastCpu fast_cpu;

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret = derive_encryption_key(
		key, params->enc_privkey_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	uint8_t privkey[FP_PK_EC_PRIVATE_KEY_LEN];

	memcpy(privkey, params->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN);

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, privkey, privkey,
			      FP_PK_EC_PRIVATE_KEY_LEN,
			      params->enc_privkey_info.nonce, FP_PK_NONCE_BYTES,
			      params->enc_privkey_info.tag, FP_PK_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to decipher template");
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_GROUP> group(
		EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));

	if (group == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> private_key(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (private_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> public_key_x(BN_bin2bn(
		params->peers_pubkey_x, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (public_key_x == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> public_key_y(BN_bin2bn(
		params->peers_pubkey_y, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (public_key_y == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_set_affine_coordinates_GFp(
		    group.get(), public_point.get(), public_key_x.get(),
		    public_key_y.get(), nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), nullptr,
			 public_point.get(), private_key.get(), nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_get_affine_coordinates_GFp(
		    group.get(), public_point.get(), public_key_x.get(),
		    public_key_y.get(), nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	if (BN_bn2binpad(public_key_x.get(), r->enc_pk, FP_PK_LEN) !=
	    FP_PK_LEN) {
		return EC_RES_UNAVAILABLE;
	}

	struct sha256_ctx ctx;

	SHA256_init(&ctx);
	SHA256_update(&ctx, r->enc_pk, FP_PK_LEN);
	uint8_t *pk = SHA256_final(&ctx);

	memcpy(r->enc_pk, pk, FP_PK_LEN);

	/* Clear the context that contain pk. */
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	r->enc_pk_info.struct_version = FP_PK_ENC_METADATA_VERSION;
	trng_init();
	trng_rand_bytes(r->enc_pk_info.nonce, FP_PK_NONCE_BYTES);
	trng_rand_bytes(r->enc_pk_info.encryption_salt,
			FP_PK_ENCRYPTION_SALT_BYTES);
	trng_exit();

	ret = derive_encryption_key(key, r->enc_pk_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(key, SBP_ENC_KEY_LEN, r->enc_pk, r->enc_pk,
			      FP_PK_LEN, r->enc_pk_info.nonce,
			      FP_PK_NONCE_BYTES, r->enc_pk_info.tag,
			      FP_PK_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_wrap: Failed to encrypt template");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PK_WRAP, fp_command_establish_pk_wrap,
		     EC_VER_MASK(0));

static enum ec_status fp_command_load_pk(struct host_cmd_handler_args *args)
{
	const auto *params =
		static_cast<const ec_params_fp_load_pk *>(args->params);

	ScopedFastCpu fast_cpu;

	/* Clear the context to prevent leaking the existing template. */
	fp_clear_context();

	uint8_t key[SBP_ENC_KEY_LEN];
	int ret =
		derive_encryption_key(key, params->enc_pk_info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_load: Failed to derive key");
		return EC_RES_UNAVAILABLE;
	}

	std::copy(params->enc_pk, params->enc_pk + FP_PK_LEN,
		  pairing_key.begin());

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, pairing_key.data(),
			      pairing_key.data(), FP_PK_EC_PRIVATE_KEY_LEN,
			      params->enc_pk_info.nonce, FP_PK_NONCE_BYTES,
			      params->enc_pk_info.tag, FP_PK_TAG_BYTES);
	OPENSSL_cleanse(key, sizeof(key));
	if (ret != EC_SUCCESS) {
		CPRINTS("pk_load: Failed to decipher pk");
		return EC_RES_UNAVAILABLE;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_LOAD_PK, fp_command_load_pk, EC_VER_MASK(0));
