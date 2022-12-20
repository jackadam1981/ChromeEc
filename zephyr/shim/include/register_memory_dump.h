/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __REGISTER_MEMORY_DUMP_H
#define __REGISTER_MEMORY_DUMP_H

/**
 * Register memory for access by memory dump host commands.
 * Must be called once before attempting a memory dump.
 * Typically called during early boot.
 *
 * @returns EC_SUCCESS or EC_XXX on error.
 */
int register_thread_memory_dump(void);

#endif /* __REGISTER_MEMORY_DUMP_H */
