/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_AUTH_CRYPTO_H
#define __CROS_EC_FPSENSOR_AUTH_CRYPTO_H

#include "openssl/ec.h"

extern "C" {
#include "ec_commands.h"
}

/**
 * Fill the @p ec_fp_ec_public_key with the content of boringssl @p EC_KEY.
 *
 * @param[in] key boringssl key
 * @param[out] pubkey public key structure
 *
 * @return EC_SUCCESS on success
 * @return EC_ERROR_INVAL on error
 */
enum ec_error_list fill_pubkey(const EC_KEY &key,
			       struct ec_fp_ec_public_key &pubkey);

/**
 * Create a boringssl @EC_KEY from the @p ec_fp_ec_public_key content.
 *
 * @param[in] pubkey public key structure
 *
 * @return @p EC_KEY on success
 * @return nullptr on error
 */
bssl::UniquePtr<EC_KEY>
create_ec_key_from_pubkey(const struct ec_fp_ec_public_key &pubkey);

/**
 * Encrypt the data in place with a specific version of encryption method and
 * output the metadata and encrypted data.
 *
 * version 1 is 128 bit AES-GCM, and the encryption key is bound to the TPM
 * seed, rollback secret and user_id.
 *
 * @param[in] version the version of the encryption method
 * @param[out] info the metadata of the encryption output
 * @param[in,out] data the data that need to be encrypted in place
 * @param[in] data_size the size of data
 *
 * @return EC_SUCCESS on success
 * @return EC_ERROR_INVAL on error
 */
enum ec_error_list
encrypt_data_in_place(uint16_t version,
		      struct ec_fp_auth_command_encryption_metadata &info,
		      uint8_t *data, size_t data_size);

/**
 * Encrypt the @p EC_KEY with a specific version of encryption method and output
 * the result in @p ec_fp_encrypted_private_key.
 *
 * version 1 is 128 bit AES-GCM, and the encryption key is bound to the TPM
 * seed, rollback secret and user_id.
 *
 * @param[in] key the private
 * @param[in] version the version of the encryption method
 * @param[out] enc_key the encryption output
 *
 * @return EC_SUCCESS on success
 * @return EC_ERROR_INVAL on error
 */
enum ec_error_list
fill_encrypted_private_key(const EC_KEY &key, uint16_t version,
			   struct ec_fp_encrypted_private_key &enc_key);

#endif /* __CROS_EC_FPSENSOR_AUTH_CRYPTO_H */
