/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __EC_INCLUDE_NVMEM_VARS_H
#define __EC_INCLUDE_NVMEM_VARS_H

/*
 * CONFIG_FLASH_NVMEM provides persistent, atomic-update storage in
 * flash. The storage is logically divided into one or more "user regions", as
 * configured in board.h and board.c
 *
 * When enabled with CONFIG_FLASH_NVMEM_VARS, a set of string-based KEY=VALUE
 * variable pairs is provided, using the nvmem user region specified by
 * CONFIG_FLASH_NVMEM_VARS_USER_NUM.
 *
 * Both KEY and VALUE are null-terminated strings. Neither may contain a '\0'
 * character, and the KEY may not contain a '=' character. Any other uint8_t
 * character is acceptable. Zero-length KEYs are not allowed. Assigning a NULL
 * or zero-length VALUE to a KEY just deletes that KEY (if it existed).
 *
 * The expected usage is:
 *
 * 1. At boot, call initvars() to ensure that the variable storage region is
 *    valid. If it isn't, it will be initialized to an empty set.
 *
 * 2. Call getenv() or setenv() as needed. The first call to either will copy
 *    the storage regsion from flash into a RAM buffer. Any changes made with
 *    setenv() will affect only that RAM buffer.
 *
 * 3. Call writevars() to commit the RAM buffer to flash and free it.
 *
 * CAUTION: The underlying CONFIG_FLASH_NVMEM implementation supports multiple
 * tasks, provided each task access only one user region. There is no support
 * for simultaneous access to the *same* user region by multiple tasks. If that
 * is required, callers should establish their own locks or mutexes to fit
 * their usage. In general that would mean aquiring a lock before calling
 * getvar() or setvar(), and releasing it after calling writevars().
 *
 * Just FYI, variables are stored in RAM and flash like so:
 *
 *   KEY1=VAL1\0KEY2=VAL2\0...KEYn=VALn\0\0
 *
 * The end of the data is indicated by the extra '\0' at the end. The overhead
 * is thus two bytes (one '=' and one '\0') for every variable, plus one more
 * byte for the final '\0'. An empty set of variables is represented as \0\0.
 *
 * There is no checksum or versioning. CONFIG_FLASH_NVMEM provides that.
 */

/* Initialize the persistent storage if needed. Should return EC_SUCCESS. */
int initvars(void);

/*
 * Look up the key, return the value. If there is no matching key, return NULL.
 *
 * WARNING: The returned pointer is only valid until the next call to setvar()
 * or writevars(). Use it or lose it.
 */
const char *getvar(const char *key);

/*
 * Set the key to the value, in RAM. If the value is NULL or "", the key is
 * deleted. Return EC_SUCCESS or error code on failure.
 */
int setvar(const char *key, const char *val);

/*
 * Commit any changes made with setvar() to persistent memory, and invalidate
 * the RAM buffer. Return EC_SUCCESS or error code on failure.
 */
int writevars(void);

#endif	/* __EC_INCLUDE_NVMEM_VARS_H */
