/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_RW_SAFE_MODE_H
#define __CROS_EC_RW_SAFE_MODE_H

#include "task_id.h"

#define SAFE_MODE_TIMEOUT_MSEC 2000

/**
 * Checks if running in rw safe mode
 *
 * @return True if system is running in rw safe mode
 */
bool system_is_in_rw_safe_mode(void);

/**
 * Checks if command is allowed in rw safe mode
 *
 * @return True if command is allowed in rw safe mode
 */
bool command_is_allowed_in_rw_safe_mode(int command);

/**
 * Start rw safe mode.
 *
 * RW safe mode can only be started after a panic in RW image.
 * It will only run briefly so the AP can capture EC state.
 *
 * @return EC_SUCCESS or EC_xxx on error
 */
int start_rw_safe_mode(void);

#ifdef TEST_BUILD
/**
 * Directly set safe mode flag. Only used in tests.
 */
void set_safe_mode(bool mode);
#endif

#endif /* __CROS_EC_RW_SAFE_MODE_H */
