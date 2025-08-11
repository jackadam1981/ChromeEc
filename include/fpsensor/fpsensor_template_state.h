/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_TEMPLATE_STATE_H
#define __CROS_EC_FPSENSOR_TEMPLATE_STATE_H

#include "ec_commands.h"
#include "fpsensor_driver.h"

#include <stdbool.h>

#include <array>
#include <variant>

/* The extra information for the decrypted template. */
struct fp_decrypted_template_state {
	/* The user_id that will be used to check the unlock template request is
	 * valid or not. */
	std::array<uint8_t, FP_CONTEXT_USERID_BYTES> user_id;
};

/* The template can only be one of these states.
 * Note: std::monostate means there is nothing in this template. */
using fp_template_state =
	std::variant<std::monostate, fp_decrypted_template_state>;

#endif /* __CROS_EC_FPSENSOR_TEMPLATE_STATE_H */
