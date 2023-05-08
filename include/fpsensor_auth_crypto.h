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
 * Create a boringssl @EC_KEY from a private key.
 *
 * @param[in] privkey private key
 *
 * @return @p EC_KEY on success
 * @return nullptr on error
 */
bssl::UniquePtr<EC_KEY> create_ec_key_from_privkey(const uint8_t *privkey,
						   size_t privkey_size);

#endif /* __CROS_EC_FPSENSOR_AUTH_CRYPTO_H */
