/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "ish_persistent_data.h"
#include "stdbool.h"

/* Is the persistent data valid? */
bool ish_persistent_data_is_valid(void)
{
	if (IS_ENABLED(CONFIG_LOW_POWER_IDLE))
		return ish_persistent_data.magic == PERSISTENT_DATA_MAGIC;
	return false;
}

/* Make the persistent data valid (to be called at reset) */
void ish_persistent_data_mark_valid(void)
{
	ish_persistent_data.magic = PERSISTENT_DATA_MAGIC;
}
