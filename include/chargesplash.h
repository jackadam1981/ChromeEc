/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "config.h"

/**
 * chargesplash_get_boot_mode() - Return true if the chargesplash UI
 * is requested at boot
 */
#ifdef CONFIG_CHARGESPLASH
bool chargesplash_get_boot_mode(void);
#else
static inline bool chargesplash_get_boot_mode(void)
{
	return false;
}
#endif
