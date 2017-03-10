/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define CROS_EC_ROLLBACK_COOKIE 0x0b112233

#ifndef __ASSEMBLER__

/**
 * Get minimum version set by rollback protection blocks.
 *
 * @return 0 if neither block is initialized, INT32_MAX on error.
 */
int rollback_get_minimum_version(void);

/**
 * Update rollback protection block to the version passed as parameter.
 *
 * @param next_min_version	Mininum version to write in rollback block.
 *
 * @return EC_SUCCESS on success, EC_ERROR_* on error.
 */
int rollback_update(uint32_t next_min_version);

/**
 * Lock rollback protection block, reboot if necessary.
 *
 * @return EC_SUCCESS if rollback was already protected.
 */
int rollback_lock(void);

#endif
