/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "crypto/elliptic_curve_key.h"
#include "openssl/aes.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"

#include <assert.h>

#include <utility>

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

/* These must be included after the "openssl/aes.h" */
#include "crypto/fipsmodule/aes/internal.h"
#include "crypto/fipsmodule/modes/internal.h"

#include <algorithm>
#include <array>
#include <iterator>
#include <type_traits>

/* Store the intermediate encrypted data for transfer & reuse purpose.*/
/* The data will be copied into fp_enc_buffer after commit. */
static uint8_t fp_xfer_buffer[FP_MAX_FINGER_COUNT]
			     [FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE];

/* The GSC paring key. */
static std::array<uint8_t, FP_PAIRING_KEY_LEN> pairing_key;

/* The auth nonce for CK. */
static std::array<uint8_t, FP_CK_AUTH_NONCE_LEN> auth_nonce;

/**
 * Clear all fingerprint templates associated with the current user id.
 */
void fp_clear_context(void)
{
	templ_valid = 0;
	templ_dirty = 0;
	template_newly_enrolled = FP_NO_SUCH_TEMPLATE;
	fp_encryption_status &= FP_ENC_STATUS_SEED_SET;
	OPENSSL_cleanse(fp_enc_buffer, sizeof(fp_enc_buffer));
	OPENSSL_cleanse(user_id, sizeof(user_id));
	OPENSSL_cleanse(auth_nonce.data(), auth_nonce.size());
	fp_disable_positive_match_secret(&positive_match_secret_state);
	for (uint16_t idx = 0; idx < FP_MAX_FINGER_COUNT; idx++)
		fp_clear_finger_context(idx);
}

enum ec_error_list fill_pubkey(const EC_KEY &key,
			       struct ec_fp_ec_public_key &pubkey)
{
	static_assert(sizeof(pubkey) == sizeof(pubkey.x) + sizeof(pubkey.y));

	/* POINT_CONVERSION_UNCOMPRESSED indicates that the point is encoded as
	 * z||x||y, where z is the octet 0x04. */
	uint8_t *pubkey_ptr = nullptr;
	if (EC_KEY_key2buf(&key, POINT_CONVERSION_UNCOMPRESSED, &pubkey_ptr,
			   nullptr) !=
	    sizeof(pubkey.x) + sizeof(pubkey.y) + 1) {
		return EC_ERROR_INVAL;
	}

	bssl::UniquePtr<uint8_t> pubkey_data(pubkey_ptr);
	memcpy(&pubkey, pubkey_data.get() + 1,
	       sizeof(pubkey.x) + sizeof(pubkey.y));

	return EC_SUCCESS;
}

bssl::UniquePtr<EC_KEY>
create_ec_key_from_pubkey(const struct ec_fp_ec_public_key &pubkey)
{
	bssl::UniquePtr<BIGNUM> x_bn(
		BN_bin2bn(pubkey.x, sizeof(pubkey.x), nullptr));
	if (x_bn == nullptr) {
		return nullptr;
	}

	bssl::UniquePtr<BIGNUM> y_bn(
		BN_bin2bn(pubkey.y, sizeof(pubkey.y), nullptr));
	if (y_bn == nullptr) {
		return nullptr;
	}

	static_assert(sizeof(pubkey.x) == 32);
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));

	if (EC_KEY_set_public_key_affine_coordinates(key.get(), x_bn.get(),
						     y_bn.get()) != 1) {
		return nullptr;
	}

	return key;
}

enum ec_error_list
encrypt_data_in_place(uint16_t version,
		      struct ec_fp_auth_command_encryption_metadata &info,
		      uint8_t *data, size_t data_size)
{
	info.struct_version = version;
	trng_init();
	trng_rand_bytes(info.nonce, sizeof(info.nonce));
	trng_rand_bytes(info.encryption_salt, sizeof(info.encryption_salt));
	trng_exit();

	uint8_t enc_key[SBP_ENC_KEY_LEN];
	enum ec_error_list ret =
		derive_encryption_key(enc_key, info.encryption_salt);
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(enc_key, SBP_ENC_KEY_LEN, data, data, data_size,
			      info.nonce, sizeof(info.nonce), info.tag,
			      sizeof(info.tag));
	OPENSSL_cleanse(enc_key, sizeof(enc_key));
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

enum ec_error_list
fill_encrypted_private_key(const EC_KEY &key, uint16_t version,
			   struct ec_fp_encrypted_private_key &enc_key)
{
	if (EC_KEY_priv2oct(&key, enc_key.data, sizeof(enc_key.data)) !=
	    sizeof(enc_key.data)) {
		return EC_ERROR_INVAL;
	}

	return encrypt_data_in_place(version, enc_key.info, enc_key.data,
				     sizeof(enc_key.data));
}

enum ec_error_list
decrypt_data(const struct ec_fp_auth_command_encryption_metadata &info,
	     const uint8_t *enc_data, size_t enc_data_size, uint8_t *data,
	     size_t data_size)
{
	uint8_t enc_key[SBP_ENC_KEY_LEN];
	enum ec_error_list ret =
		derive_encryption_key(enc_key, info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to derive key");
		return EC_ERROR_INVAL;
	}

	if (enc_data_size != data_size) {
		CPRINTS("Data size mismatch");
		return EC_ERROR_INVAL;
	}

	ret = aes_gcm_decrypt(enc_key, SBP_ENC_KEY_LEN, data, enc_data,
			      data_size, info.nonce, sizeof(info.nonce),
			      info.tag, sizeof(info.tag));
	OPENSSL_cleanse(enc_key, sizeof(enc_key));
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to decipher data");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

bssl::UniquePtr<EC_KEY> decrypt_private_key(
	const struct ec_fp_encrypted_private_key &encrypted_private_key)
{
	uint8_t privkey[sizeof(encrypted_private_key.data)];

	enum ec_error_list ret = decrypt_data(encrypted_private_key.info,
					      encrypted_private_key.data,
					      sizeof(privkey), privkey,
					      sizeof(privkey));
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to decrypt private key");
		return nullptr;
	}

	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		return nullptr;
	}

	if (EC_KEY_oct2priv(key.get(), privkey, sizeof(privkey)) != 1) {
		return nullptr;
	}

	return key;
}

enum ec_error_list generate_ecdh_shared_secret(const EC_KEY &private_key,
					       const EC_KEY &public_key,
					       uint8_t *shared_secret,
					       uint8_t shared_secret_size)
{
	const EC_POINT *public_point = EC_KEY_get0_public_key(&public_key);
	if (public_point == nullptr) {
		return EC_ERROR_INVAL;
	}

	if (ECDH_compute_key_fips(shared_secret, shared_secret_size,
				  public_point, &private_key) != 1) {
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

static enum ec_status
fp_command_establish_pairing_key_keygen(struct host_cmd_handler_args *args)
{
	auto *r = static_cast<ec_response_fp_establish_pairing_key_keygen *>(
		args->response);

	ScopedFastCpu fast_cpu;

	bssl::UniquePtr<EC_KEY> ecdh_key = generate_elliptic_curve_key();
	if (ecdh_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	enum ec_error_list res = fill_encrypted_private_key(
		*ecdh_key, FP_PAIRING_KEY_ENC_METADATA_VERSION,
		r->encrypted_private_key);
	if (res != EC_SUCCESS) {
		CPRINTS("pairing_keygen: Failed to fill response encrypted private key");
		return EC_RES_UNAVAILABLE;
	}

	res = fill_pubkey(*ecdh_key, r->pubkey);
	if (res != EC_SUCCESS) {
		CPRINTS("pairing_keygen: Failed to fill response pubkey");
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

	bssl::UniquePtr<EC_KEY> private_key =
		decrypt_private_key(params->encrypted_private_key);
	if (private_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_KEY> public_key =
		create_ec_key_from_pubkey(params->peers_pubkey);
	if (public_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	enum ec_error_list ret = generate_ecdh_shared_secret(
		*private_key, *public_key, r->encrypted_pairing_key.data,
		sizeof(r->encrypted_pairing_key.data));
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to compute ECDH share secret");
		return EC_RES_UNAVAILABLE;
	}

	ret = encrypt_data_in_place(FP_PAIRING_KEY_ENC_METADATA_VERSION,
				    r->encrypted_pairing_key.info,
				    r->encrypted_pairing_key.data,
				    sizeof(r->encrypted_pairing_key.data));
	if (ret != EC_SUCCESS) {
		CPRINTS("pairing_key_wrap: Failed to encrypt pairing key");
		return EC_RES_UNAVAILABLE;
	}

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_ESTABLISH_PAIRING_KEY_WRAP,
		     fp_command_establish_pairing_key_wrap, EC_VER_MASK(0));

static enum ec_status
fp_command_load_pairing_key(struct host_cmd_handler_args *args)
{
	const auto *params = static_cast<const ec_params_fp_load_pairing_key *>(
		args->params);

	ScopedFastCpu fast_cpu;

	/* Clear the context to prevent leaking the existing template. */
	fp_clear_context();

	enum ec_error_list ret =
		decrypt_data(params->encrypted_pairing_key.info,
			     params->encrypted_pairing_key.data,
			     sizeof(params->encrypted_pairing_key.data),
			     pairing_key.data(), pairing_key.size());
	if (ret != EC_SUCCESS) {
		CPRINTS("load_pairing_key: Failed to decrypt pairing key");
		return EC_RES_UNAVAILABLE;
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_LOAD_PAIRING_KEY, fp_command_load_pairing_key,
		     EC_VER_MASK(0));

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
	trng_rand_bytes(auth_nonce.data(), auth_nonce.size());
	trng_exit();

	std::copy(auth_nonce.begin(), auth_nonce.end(), r->nonce);

	fp_encryption_status |= FP_CONTEXT_AUTH_NONCE_SET;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_GENERATE_NONCE, fp_command_generate_nonce,
		     EC_VER_MASK(0));

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
	SHA256_update(&ctx, auth_nonce.data(), auth_nonce.size());
	SHA256_update(&ctx, p->gsc_nonce, FP_CK_AUTH_NONCE_LEN);
	SHA256_update(&ctx, pairing_key.data(), pairing_key.size());
	uint8_t *ck = SHA256_final(&ctx);

	AES_KEY aes_key;
	int res = AES_set_encrypt_key(ck, 256, &aes_key);
	if (res) {
		CPRINTS("Failed to set encryption key: %d", res);
		return EC_RES_UNAVAILABLE;
	}

	uint8_t aes_iv[sizeof(p->enc_user_id_iv)];
	static_assert(sizeof(p->enc_user_id_iv) == AES_BLOCK_SIZE);
	memcpy(aes_iv, p->enc_user_id_iv, sizeof(aes_iv));

	/* The AES CTR used the same function for encryption & decryption. */
	unsigned int block_num = 0;
	uint8_t ecount_buf[AES_BLOCK_SIZE];
	uint8_t raw_user_id[sizeof(user_id)];
	static_assert(sizeof(p->enc_user_id) == sizeof(user_id));
	AES_ctr128_encrypt(p->enc_user_id, raw_user_id, sizeof(raw_user_id),
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

static enum ec_status
fp_command_read_match_secret_with_pubkey(struct host_cmd_handler_args *args)
{
	const auto *params =
		static_cast<const ec_params_fp_read_match_secret_with_pubkey *>(
			args->params);
	auto *response =
		static_cast<ec_response_fp_read_match_secret_with_pubkey *>(
			args->response);
	int8_t fgr = params->fgr;

	ScopedFastCpu fast_cpu;

	bssl::UniquePtr<EC_KEY> private_key = generate_elliptic_curve_key();
	if (private_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	enum ec_error_list ret = fill_pubkey(*private_key, response->pubkey);
	if (ret != EC_SUCCESS) {
		CPRINTS("read_match_secret: Failed to fill response pubkey");
		return EC_RES_UNAVAILABLE;
	}

	bssl::UniquePtr<EC_KEY> public_key =
		create_ec_key_from_pubkey(params->pubkey);
	if (public_key == nullptr) {
		return EC_RES_UNAVAILABLE;
	}

	uint8_t enc_key[SHA256_DIGEST_SIZE];

	ret = generate_ecdh_shared_secret(*private_key, *public_key, enc_key,
					  sizeof(enc_key));
	if (ret != EC_SUCCESS) {
		CPRINTS("read_match_secret: Failed to compute ECDH share secret");
		return EC_RES_UNAVAILABLE;
	}

	AES_KEY aes_key;
	int res = AES_set_encrypt_key(enc_key, 256, &aes_key);
	if (res) {
		CPRINTS("read_match_secret: Failed to set encryption key: %d",
			res);
		return EC_RES_UNAVAILABLE;
	}

	static_assert(sizeof(response->iv) == AES_BLOCK_SIZE);

	trng_init();
	trng_rand_bytes(response->iv, FP_EC_PUBLIC_KEY_IV_LEN);
	trng_exit();

	/* The IV would be changed after the AES_ctr128_encrypt, we need a copy
	 * for that. */
	uint8_t aes_iv[sizeof(response->iv)];

	memcpy(aes_iv, response->iv, sizeof(aes_iv));

	static_assert(sizeof(response->enc_secret) ==
		      FP_POSITIVE_MATCH_SECRET_BYTES);

	enum ec_status status = fp_read_match_secret(fgr, response->enc_secret);
	if (status != EC_RES_SUCCESS) {
		return status;
	}

	unsigned int block_num = 0;
	uint8_t ecount_buf[AES_BLOCK_SIZE];

	/* The AES CTR used the same function for encryption & decryption. */
	AES_ctr128_encrypt(response->enc_secret, response->enc_secret,
			   sizeof(response->enc_secret), &aes_key, aes_iv,
			   ecount_buf, &block_num);

	/* Clear the key materials. */
	OPENSSL_cleanse(&enc_key, sizeof(enc_key));
	OPENSSL_cleanse(&aes_key, sizeof(aes_key));

	args->response_size = sizeof(*response);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_READ_MATCH_SECRET_WITH_PUBKEY,
		     fp_command_read_match_secret_with_pubkey, EC_VER_MASK(0));

static enum ec_status
fp_command_preload_template(struct host_cmd_handler_args *args)
{
	const auto *params = static_cast<const ec_params_fp_preload_template *>(
		args->params);

	ScopedFastCpu fast_cpu;

	uint32_t size = params->size & ~FP_TEMPLATE_COMMIT;
	int xfer_complete = params->size & FP_TEMPLATE_COMMIT;
	uint32_t offset = params->offset;
	uint16_t idx = params->fgr;

	/* Can we store one more template ? */
	if (idx >= FP_MAX_FINGER_COUNT)
		return EC_RES_OVERFLOW;

	if (args->params_size !=
	    size + offsetof(struct ec_params_fp_preload_template, data))
		return EC_RES_INVALID_PARAM;

	enum ec_error_list ret = validate_fp_buffer_offset(
		sizeof(fp_xfer_buffer[idx]), offset, size);
	if (ret != EC_SUCCESS)
		return EC_RES_INVALID_PARAM;

	memcpy(&fp_xfer_buffer[idx][offset], params->data, size);

	if (xfer_complete) {
		memcpy(fp_enc_buffer, fp_xfer_buffer[idx],
		       FP_ALGORITHM_ENCRYPTED_TEMPLATE_SIZE);
	}

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_PRELOAD_TEMPLATE, fp_command_preload_template,
		     EC_VER_MASK(0));
