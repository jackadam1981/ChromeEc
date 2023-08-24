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

struct fp_template_state {
	struct encrypted_state {
		ec_fp_template_encryption_metadata enc_metadata;
	};

	struct decrypted_state {
		bool is_locked;
		std::array<uint32_t, FP_CONTEXT_USERID_WORDS> user_id;
	};

	std::variant<std::monostate, decrypted_state, encrypted_state> state;
};

extern std::array<fp_template_state, FP_MAX_FINGER_COUNT> template_states;

#endif /* __CROS_EC_FPSENSOR_TEMPLATE_STATE_H */
