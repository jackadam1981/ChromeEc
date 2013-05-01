/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* System module for emulator */

#include <stdlib.h>
#include "system.h"

void system_reset(int flags)
{
	exit((1 << 6) | flags);
}

void system_hibernate(uint32_t seconds, uint32_t microseconds)
{
	exit(1 << 7);
}

int system_is_locked(void)
{
	return 0;
}

int system_jumped_to_this_image(void)
{
	return 0;
}

uint32_t system_get_reset_flags(void)
{
	return RESET_FLAG_POWER_ON;
}
