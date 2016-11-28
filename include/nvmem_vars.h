/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_INCLUDE_NVMEM_VARS_H
#define __EC_INCLUDE_NVMEM_VARS_H


/*
 * Keys and values are stored as null-terminated KEY=VALUE strings.
 * Keys may not contain '=' characters. Zero-length values are not allowed.
 *
 * Data are saved in a persistent user buffer implemented using
 * CONFIG_FLASH_NVMEM. The first call to either getenv() or setenv() will copy
 * the persistent storage content into RAM and will use only that RAM copy
 * until explicitly committed with writeenv().
 */

/* Initialize the persistent storage if needed. Should return EC_SUCCESS. */
int initenv(void);

/*
 * Look up the key, return the value. If there is no matching key, return NULL.
 *
 * WARNING: The returned pointer is only valid until the next call to setenv()
 * or writeenv().
 */
const char *getenv(const char *key);

/*
 * Set the key to the value, in RAM. If the value is NULL or "", the key is
 * deleted. Return EC_SUCCESS or error code on failure.
 */
int setenv(const char *key, const char *val);

/*
 * Commit any changes made with setenv() to persistent memory, and invalidate
 * the RAM copy. Return EC_SUCCESS or error code on failure.
 */
int writeenv(void);

#endif	/* __EC_INCLUDE_NVMEM_VARS_H */
