/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Trulo hdmi sub board configuration */

#include "hooks.h"
#include "keyboard_scan.h"
#include "tablet_mode.h"

void handle_chipset_resume(void)
{
	/*
	 * Enable keyboard when AP is running.
	 */
	keyboard_scan_enable(1, KB_SCAN_DISABLE_LID_ANGLE);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, handle_chipset_resume, HOOK_PRIO_DEFAULT);

void handle_chipset_suspend(void)
{
	/*
	 * Disable keyboard in tablet mode when AP is suspended
	 */
	if (tablet_get_mode()) {
		keyboard_scan_enable(0, KB_SCAN_DISABLE_LID_ANGLE);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, handle_chipset_suspend, HOOK_PRIO_DEFAULT);
