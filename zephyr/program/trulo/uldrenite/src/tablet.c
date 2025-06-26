/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "hooks.h"
#include "lid_angle.h"
#include "tablet_mode.h"

/* Add enable peripherals when mode change to clameshell, avoid clameshell mode
 * lock Keyboard by disable_scanning_mask.
 */
LOG_MODULE_REGISTER(uldrenite_tablet, LOG_LEVEL_INF);

void clameshell_enable_peripherals(void)
{
	if (!tablet_get_mode())
		lid_angle_peripheral_enable(1);
}
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, clameshell_enable_peripherals,
	     HOOK_PRIO_DEFAULT);
