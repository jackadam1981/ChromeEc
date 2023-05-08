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

#endif /* __CROS_EC_FPSENSOR_AUTH_COMMANDS_H */
