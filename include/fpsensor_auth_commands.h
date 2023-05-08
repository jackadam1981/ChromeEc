/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_AUTH_COMMANDS_H
#define __CROS_EC_FPSENSOR_AUTH_COMMANDS_H

#include "openssl/ec.h"

extern "C" {
#include "ec_commands.h"
}

/**
 * Clear all fingerprint templates associated with the current user id.
 */
void fp_clear_context(void);

enum ec_error_list fill_pubkey(const EC_KEY &key,
			       struct ec_fp_ec_public_key &pubkey);

bssl::UniquePtr<EC_KEY>
create_ec_key_from_pubkey(const struct ec_fp_ec_public_key &pubkey);

enum ec_error_list
encrypt_data_in_place(uint16_t version,
		      struct ec_fp_auth_command_encryption_metadata &info,
		      uint8_t *data, size_t data_size);

enum ec_error_list
fill_encrypted_private_key(const EC_KEY &key, uint16_t version,
			   struct ec_fp_encrypted_private_key &enc_key);

enum ec_error_list
decrypt_data(const struct ec_fp_auth_command_encryption_metadata &info,
	     const uint8_t *enc_data, size_t enc_data_size, uint8_t *data,
	     size_t data_size);

bssl::UniquePtr<EC_KEY> decrypt_private_key(
	const struct ec_fp_encrypted_private_key &encrypted_private_key);

enum ec_error_list generate_ecdh_shared_secret(const EC_KEY &private_key,
					       const EC_KEY &public_key,
					       uint8_t *share_secret,
					       uint8_t share_secret_size);

#endif /* __CROS_EC_FPSENSOR_AUTH_COMMANDS_H */
