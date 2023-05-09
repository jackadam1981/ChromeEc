/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "compile_time_macros.h"

/* Boringssl headers need to be included before extern "C" section. */
#include "crypto/cleanse_wrapper.h"
#include "crypto/elliptic_curve_key.h"
#include "openssl/aes.h"
#include "openssl/bn.h"
#include "openssl/ec.h"
#include "openssl/ecdh.h"
#include "openssl/mem.h"
#include "openssl/obj_mac.h"
#include "openssl/rand.h"

extern "C" {
#include "ec_commands.h"
#include "sha256.h"
}

#include "fpsensor.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state_without_driver_info.h"
#include "fpsensor_utils.h"

#include <array>

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

bssl::UniquePtr<EC_KEY> create_ec_key_from_privkey(const uint8_t *privkey,
						   size_t privkey_size)
{
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		return nullptr;
	}

	if (EC_KEY_oct2priv(key.get(), privkey, privkey_size) != 1) {
		return nullptr;
	}

	return key;
}

enum ec_error_list
encrypt_data_in_place(uint16_t version,
		      struct ec_fp_auth_command_encryption_metadata &info,
		      uint8_t *data, size_t data_size)
{
	if (version != 1) {
		return EC_ERROR_INVAL;
	}

	info.struct_version = version;
	RAND_bytes(info.nonce, sizeof(info.nonce));
	RAND_bytes(info.encryption_salt, sizeof(info.encryption_salt));

	CleanseWrapper<std::array<uint8_t, SBP_ENC_KEY_LEN> > enc_key;
	enum ec_error_list ret =
		derive_encryption_key(enc_key.data(), info.encryption_salt);
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	/* Encrypt the secret blob in-place. */
	ret = aes_gcm_encrypt(enc_key.data(), enc_key.size(), data, data,
			      data_size, info.nonce, sizeof(info.nonce),
			      info.tag, sizeof(info.tag));
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
	if (info.struct_version != 1) {
		return EC_ERROR_INVAL;
	}

	CleanseWrapper<std::array<uint8_t, SBP_ENC_KEY_LEN> > enc_key;
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
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to decipher data");
		return EC_ERROR_INVAL;
	}

	return EC_SUCCESS;
}

bssl::UniquePtr<EC_KEY> decrypt_private_key(
	const struct ec_fp_encrypted_private_key &encrypted_private_key)
{
	CleanseWrapper<std::array<uint8_t, sizeof(encrypted_private_key.data)> >
		privkey;

	enum ec_error_list ret = decrypt_data(
		encrypted_private_key.info, encrypted_private_key.data,
		sizeof(encrypted_private_key.data), privkey.data(),
		privkey.size());
	if (ret != EC_SUCCESS) {
		CPRINTS("Failed to decrypt private key");
		return nullptr;
	}

	return create_ec_key_from_privkey(privkey.data(), privkey.size());
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

enum ec_error_list
generate_gsc_session_key(const uint8_t *auth_nonce, size_t auth_nonce_size,
			 const uint8_t *gsc_nonce, size_t gsc_nonce_size,
			 const uint8_t *pairing_key, size_t pairing_key_size,
			 uint8_t *gsc_session_key, size_t gsc_session_key_size)
{
	if (auth_nonce_size != 32 || gsc_nonce_size != 32 ||
	    pairing_key_size != 32 ||
	    gsc_session_key_size != SHA256_DIGEST_SIZE) {
		return EC_ERROR_INVAL;
	}
	CleanseWrapper<struct sha256_ctx> ctx;
	SHA256_init(&ctx);
	SHA256_update(&ctx, auth_nonce, auth_nonce_size);
	SHA256_update(&ctx, gsc_nonce, gsc_nonce_size);
	SHA256_update(&ctx, pairing_key, pairing_key_size);
	uint8_t *result = SHA256_final(&ctx);

	std::copy(result, result + SHA256_DIGEST_SIZE, gsc_session_key);

	return EC_SUCCESS;
}

enum ec_error_list decrypt_data_with_gsc_session_key_in_place(
	const uint8_t *gsc_session_key, size_t gsc_session_key_size,
	const uint8_t *iv, size_t iv_size, uint8_t *data, size_t data_size)
{
	if (gsc_session_key_size != 32 || iv_size != AES_BLOCK_SIZE) {
		return EC_ERROR_INVAL;
	}

	CleanseWrapper<AES_KEY> aes_key;
	int res = AES_set_encrypt_key(gsc_session_key, 256, &aes_key);
	if (res) {
		return EC_ERROR_INVAL;
	}

	std::array<uint8_t, AES_BLOCK_SIZE> aes_iv;
	std::copy(iv, iv + iv_size, aes_iv.begin());

	/* The AES CTR used the same function for encryption & decryption. */
	unsigned int block_num = 0;
	std::array<uint8_t, AES_BLOCK_SIZE> ecount_buf;
	AES_ctr128_encrypt(data, data, data_size, &aes_key, aes_iv.data(),
			   ecount_buf.data(), &block_num);

	return EC_SUCCESS;
}

enum ec_error_list
encrypt_data_with_ecdh_key_in_place(const struct ec_fp_ec_public_key &in_pubkey,
				    struct ec_fp_ec_public_key &out_pubkey,
				    uint8_t *data, size_t data_size,
				    uint8_t *iv, size_t iv_size)
{
	if (iv_size != AES_BLOCK_SIZE) {
		return EC_ERROR_INVAL;
	}

	bssl::UniquePtr<EC_KEY> private_key = generate_elliptic_curve_key();
	if (private_key == nullptr) {
		return EC_ERROR_INVAL;
	}

	enum ec_error_list ret = fill_pubkey(*private_key, out_pubkey);
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	bssl::UniquePtr<EC_KEY> public_key =
		create_ec_key_from_pubkey(in_pubkey);
	if (public_key == nullptr) {
		return EC_ERROR_INVAL;
	}

	CleanseWrapper<std::array<uint8_t, SHA256_DIGEST_SIZE> > enc_key;

	ret = generate_ecdh_shared_secret(*private_key, *public_key,
					  enc_key.data(), enc_key.size());
	if (ret != EC_SUCCESS) {
		return EC_ERROR_INVAL;
	}

	CleanseWrapper<AES_KEY> aes_key;
	int res = AES_set_encrypt_key(enc_key.data(), 256, &aes_key);
	if (res) {
		return EC_ERROR_INVAL;
	}

	RAND_bytes(iv, iv_size);

	/* The IV would be changed after the AES_ctr128_encrypt, we need a copy
	 * for that. */
	std::array<uint8_t, AES_BLOCK_SIZE> aes_iv;

	std::copy(iv, iv + iv_size, aes_iv.begin());

	unsigned int block_num = 0;
	uint8_t ecount_buf[AES_BLOCK_SIZE];

	/* The AES CTR used the same function for encryption & decryption. */
	AES_ctr128_encrypt(data, data, data_size, &aes_key, aes_iv.data(),
			   ecount_buf, &block_num);

	return EC_SUCCESS;
}
