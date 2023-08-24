/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_TEMPLATE_STATE_H
#define __CROS_EC_FPSENSOR_TEMPLATE_STATE_H

#include "ec_commands.h"

#include <stdbool.h>

#include <array>
#include <variant>

extern "C" {
#include "fpsensor_driver.h"
}

struct fp_encrypted_template_state {
	ec_fp_template_encryption_metadata enc_metadata;
};

struct fp_decrypted_template_state {
	bool is_locked;
	std::array<uint32_t, FP_CONTEXT_USERID_WORDS> user_id;
};

using fp_template_state =
	std::variant<std::monostate, fp_encrypted_template_state,
		     fp_decrypted_template_state>;

extern std::array<fp_template_state, FP_MAX_FINGER_COUNT> template_states;

#endif /* __CROS_EC_FPSENSOR_TEMPLATE_STATE_H */
