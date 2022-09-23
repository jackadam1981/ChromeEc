/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_AUTH_COMMANDS_H
#define __CROS_EC_FPSENSOR_AUTH_COMMANDS_H

#include <array>

/* The auth nonce for GSC session key. */
extern std::array<uint8_t, FP_CK_AUTH_NONCE_LEN> auth_nonce;

#endif /* __CROS_EC_FPSENSOR_AUTH_COMMANDS_H */