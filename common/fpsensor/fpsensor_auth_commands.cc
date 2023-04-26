/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "crypto/elliptic_curve_key.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"

#include <assert.h>

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

/**
 * Clear all fingerprint templates associated with the current user id.
 */
void fp_clear_context(void)
{
	templ_valid = 0;
	templ_dirty = 0;
	OPENSSL_cleanse(fp_enc_buffer, sizeof(fp_enc_buffer));
	OPENSSL_cleanse(user_id, sizeof(user_id));
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
	uint8_t *data = nullptr;
	if (EC_KEY_key2buf(&key, POINT_CONVERSION_UNCOMPRESSED, &data,
			   nullptr) !=
	    sizeof(pubkey.x) + sizeof(pubkey.y) + 1) {
		return EC_ERROR_INVAL;
	}

	bssl::UniquePtr<uint8_t> pubkey_data(data);
	uint8_t *pubkey_ptr = reinterpret_cast<uint8_t *>(&pubkey);
	std::copy(pubkey_data.get() + 1,
		  pubkey_data.get() + 1 + sizeof(pubkey.x) + sizeof(pubkey.y),
		  pubkey_ptr);

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
	static_assert(sizeof(pubkey.y) == 32);
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		return nullptr;
	}

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

	std::array<uint8_t, SBP_ENC_KEY_LEN> enc_key;
	enum ec_error_list ret =
		derive_encryption_key(enc_key.data(), info.encryption_salt);
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(enc_key.data(), enc_key.size(), data, data,
			      data_size, info.nonce, sizeof(info.nonce),
			      info.tag, sizeof(info.tag));
	OPENSSL_cleanse(enc_key.data(), enc_key.size());
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
	std::array<uint8_t, SBP_ENC_KEY_LEN> enc_key;
	enum ec_error_list ret =
		derive_encryption_key(enc_key.data(), info.encryption_salt);
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to derive key");
		return EC_ERROR_INVAL;
	}

	if (enc_data_size != data_size) {
		CPRINTS("Data size mismatch");
		return EC_ERROR_INVAL;
	}

	ret = aes_gcm_decrypt(enc_key.data(), enc_key.size(), data, enc_data,
			      data_size, info.nonce, sizeof(info.nonce),
			      info.tag, sizeof(info.tag));
	OPENSSL_cleanse(enc_key.data(), enc_key.size());
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
