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

namespace
{

bssl::UniquePtr<EC_KEY> generate_elliptic_curve_key()
{
	bssl::UniquePtr<EC_KEY> key(
		EC_KEY_new_by_curve_name(NID_X9_62_prime256v1));
	if (key == nullptr) {
		return nullptr;
	}

	if (EC_KEY_generate_key(key.get()) != 1) {
		return nullptr;
	}

	return key;
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
			   struct ec_fp_auth_command_encryption_metadata &info,
			   uint8_t *data, size_t data_size)
{
	if (EC_KEY_priv2oct(&key, data, data_size) != data_size) {
		return EC_ERROR_INVAL;
	}

	return encrypt_data_in_place(version, info, data, data_size);
}

} // namespace

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
		r->encrypted_private_key.info, r->encrypted_private_key.data,
		sizeof(r->encrypted_private_key.data));
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
