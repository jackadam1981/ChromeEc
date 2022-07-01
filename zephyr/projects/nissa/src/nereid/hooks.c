/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/init.h>
#include "hooks.h"

static void board_system_suspend_hooks(void)
{
	IT8XXX2_ECPM_SCDCR0 = 1;
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_system_suspend_hooks, HOOK_PRIO_LAST);

static void board_system_resume_hooks(void)
{
	IT8XXX2_ECPM_SCDCR0 = 0;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_system_resume_hooks, HOOK_PRIO_FIRST);

static void board_clock_init(void)
{
	IT8XXX2_ECPM_CGCTRL2R = 0x10;
	IT8XXX2_ECPM_CGCTRL3R = 0x5b;
	IT8XXX2_ECPM_CGCTRL5R = 0x7a;
	IT8XXX2_ECPM_CGCTRL6R = 0xe;
}
DECLARE_HOOK(HOOK_INIT, board_clock_init, HOOK_PRIO_DEFAULT);
