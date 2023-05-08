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
 * Create a boringssl @EC_KY from the @p ec_fp_ec_public_key content.
 *
 * @param[in] pubkey public key structure
 *
 * @return @p EC_KEY on success
 * @return nullptr on error
 */
bssl::UniquePtr<EC_KEY>
create_ec_key_from_pubkey(const struct ec_fp_ec_public_key &pubkey);

#endif /* __CROS_EC_FPSENSOR_AUTH_COMMANDS_H */
