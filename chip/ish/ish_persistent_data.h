/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISH_PERSISTENT_DATA_H
#define __CROS_EC_ISH_PERSISTENT_DATA_H

#include "panic.h"
#include "stdbool.h"

#define PERSISTENT_DATA_MAGIC 0x49534864 /* "ISHd" */

struct ish_persistent_data {
	uint32_t magic;
	uint32_t reset_flags;
	uint32_t watchdog_counter;
	struct panic_data panic_data;
};

extern struct ish_persistent_data ish_persistent_data;

/* Is the persistent data valid? */
bool ish_persistent_data_is_valid(void);

/* Make the persistent data valid (to be called at reset) */
void ish_persistent_data_mark_valid(void);

#endif /* __CROS_EC_ISH_PERSISTENT_DATA_H */
