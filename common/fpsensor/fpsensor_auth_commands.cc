/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "crypto/elliptic_curve_key.h"
#include "ec_commands.h"
#include "fpsensor.h"
#include "fpsensor_auth_crypto.h"
#include "fpsensor_crypto.h"
#include "fpsensor_state.h"
#include "fpsensor_utils.h"
#include "openssl/mem.h"
#include "openssl/rand.h"
#include "scoped_fast_cpu.h"

#include <algorithm>
#include <array>

/* The GSC pairing key. */
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
	OPENSSL_cleanse(fp_enc_buffer, sizeof(fp_enc_buffer));
	OPENSSL_cleanse(user_id, sizeof(user_id));
	OPENSSL_cleanse(auth_nonce.data(), auth_nonce.size());
	fp_disable_positive_match_secret(&positive_match_secret_state);
	for (uint16_t idx = 0; idx < FP_MAX_FINGER_COUNT; idx++)
		fp_clear_finger_context(idx);
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

	RAND_bytes(auth_nonce.data(), auth_nonce.size());

	std::copy(auth_nonce.begin(), auth_nonce.end(), r->nonce);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_FP_GENERATE_NONCE, fp_command_generate_nonce,
		     EC_VER_MASK(0));
