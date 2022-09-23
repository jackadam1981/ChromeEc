/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "openssl/aes.h"
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

/* These must be included after the "openssl/aes.h" */
#include "crypto/fipsmodule/aes/internal.h"
#include "crypto/fipsmodule/modes/internal.h"

BUILD_ASSERT(FP_PK_LEN == SHA256_DIGEST_SIZE);
BUILD_ASSERT(FP_PK_EC_PUBLIC_KEY_LEN == 32);
BUILD_ASSERT(FP_PK_EC_PRIVATE_KEY_LEN == 32);

/* The GSC paring key. */
static uint8_t pairing_key[FP_PK_LEN];
/* The auth nonce for CK. */
static uint8_t auth_nonce[FP_CK_AUTH_NONCE_LEN];

/**
 * @warning |fp_buffer| contains data used by the matching algorithm that must
 * be released by calling fp_sensor_deinit() first. Call
 * fp_reset_and_clear_context instead of calling this directly.
 */
void fp_clear_context(void)
{
	templ_valid = 0;
	templ_dirty = 0;
	fp_encryption_status &= FP_ENC_STATUS_SEED_SET;
	OPENSSL_cleanse(fp_buffer, sizeof(fp_buffer));
	OPENSSL_cleanse(fp_enc_buffer, sizeof(fp_enc_buffer));
	OPENSSL_cleanse(user_id, sizeof(user_id));
	OPENSSL_cleanse(auth_nonce, sizeof(auth_nonce));
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

	if (group.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> secret(
		BN_bin2bn(r->enc_privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (secret.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	if (EC_POINT_mul(group.get(), public_point.get(), secret.get(), nullptr,
			 nullptr, nullptr) != 1) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> x_bn(BN_new());

	if (x_bn.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> y_bn(BN_new());

	if (y_bn.get() == nullptr) {
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

	if (group.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> private_key(
		BN_bin2bn(privkey, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (private_key.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> public_key_x(BN_bin2bn(
		params->peers_pubkey_x, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (public_key_x.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<BIGNUM> public_key_y(BN_bin2bn(
		params->peers_pubkey_y, FP_PK_EC_PRIVATE_KEY_LEN, nullptr));

	if (public_key_y.get() == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_POINT> public_point(EC_POINT_new(group.get()));

	if (public_point.get() == nullptr) {
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

	memcpy(pairing_key, params->enc_pk, FP_PK_LEN);

	/* Decrypt the secret blob in-place. */
	ret = aes_gcm_decrypt(key, SBP_ENC_KEY_LEN, pairing_key, pairing_key,
			      FP_PK_EC_PRIVATE_KEY_LEN,
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

static enum ec_status
fp_command_generate_nonce(struct host_cmd_handler_args *args)
{
	auto *r = static_cast<ec_response_fp_generate_nonce *>(args->response);

	ScopedFastCpu fast_cpu;

	if (fp_encryption_status & FP_CONTEXT_STATUS_NONCE_CONTEXT_SET) {
		/* Clear the context to prevent leaking the data from previous
		 * nonce context.
		 */
		fp_clear_context();
	}

	trng_init();
	trng_rand_bytes(auth_nonce, FP_CK_AUTH_NONCE_LEN);
	trng_exit();

	memcpy(r->nonce, auth_nonce, FP_CK_AUTH_NONCE_LEN);

	fp_encryption_status |= FP_CONTEXT_AUTH_NONCE_SET;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_GENERATE_NONCE, fp_command_generate_nonce,
		     EC_VER_MASK(0));

BUILD_ASSERT(FP_CONTEXT_KEY_LEN == FP_CONTEXT_USERID_LEN);
BUILD_ASSERT(FP_CONTEXT_USERID_IV_LEN == AES_BLOCK_SIZE);

static enum ec_status
fp_command_nonce_context(struct host_cmd_handler_args *args)
{
	const auto *p =
		static_cast<const ec_params_fp_nonce_context *>(args->params);

	if (!(fp_encryption_status & FP_CONTEXT_AUTH_NONCE_SET)) {
		CPRINTS("No existing auth nonce");
		return EC_RES_ACCESS_DENIED;
	}

	ScopedFastCpu fast_cpu;

	struct sha256_ctx ctx;

	SHA256_init(&ctx);
	SHA256_update(&ctx, auth_nonce, FP_CK_AUTH_NONCE_LEN);
	SHA256_update(&ctx, p->gsc_nonce, FP_CK_AUTH_NONCE_LEN);
	SHA256_update(&ctx, pairing_key, FP_PK_LEN);
	uint8_t *ck = SHA256_final(&ctx);

	AES_KEY aes_key;
	int res = AES_set_encrypt_key(ck, 256, &aes_key);

	if (res) {
		CPRINTS("Failed to set encryption key: %d", res);
		return EC_RES_UNAVAILABLE;
	}

	uint8_t aes_iv[FP_CONTEXT_USERID_IV_LEN];

	memcpy(aes_iv, p->enc_user_id_iv, FP_CONTEXT_USERID_IV_LEN);

	unsigned int block_num = 0;
	uint8_t raw_user_id[FP_CONTEXT_USERID_LEN];
	uint8_t ecount_buf[AES_BLOCK_SIZE];
	/* The AES CTR used the same function for encryption & decryption. */
	AES_ctr128_encrypt(p->enc_user_id, raw_user_id, FP_CONTEXT_USERID_LEN,
			   &aes_key, aes_iv, ecount_buf, &block_num);

	/* Clear the key material. */
	OPENSSL_cleanse(&aes_key, sizeof(aes_key));
	OPENSSL_cleanse(&ctx, sizeof(ctx));

	if (p->clear_context) {
		/* Clear the previous context. */
		fp_clear_context();
	}

	/* Set the user_id. */
	memcpy(user_id, raw_user_id, FP_CONTEXT_USERID_LEN);

	fp_encryption_status &= FP_ENC_STATUS_SEED_SET;
	fp_encryption_status |= FP_CONTEXT_STATUS_NONCE_CONTEXT_SET;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_NONCE_CONTEXT, fp_command_nonce_context,
		     EC_VER_MASK(0));
