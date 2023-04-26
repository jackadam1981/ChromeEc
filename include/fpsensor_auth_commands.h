/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fingerprint sensor interface */

#ifndef __CROS_EC_FPSENSOR_AUTH_COMMANDS_H
#define __CROS_EC_FPSENSOR_AUTH_COMMANDS_H

/**
 * Clear all fingerprint templates associated with the current user id.
 */
void fp_clear_context(void);

#endif /* __CROS_EC_FPSENSOR_AUTH_COMMANDS_H */
