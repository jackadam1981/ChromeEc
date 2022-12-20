/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Commands for dumping EC memory */

#ifndef __CROS_EC_MEMORY_DUMP_H
#define __CROS_EC_MEMORY_DUMP_H

/*
 * Initialize a memory dump.
 * This is not technically a snapshot since the memory is not strictly copied or
 * frozen. The memory could still change after the memory dump is initialized.
 * This function will decide which memory regions will be included in
 * the memory dump.
 */
int initialize_memory_dump(void);

/* Clear an previously initialized memory dump */
int clear_memory_dump(void);

#endif /* __CROS_EC_MEMORY_DUMP_H */
