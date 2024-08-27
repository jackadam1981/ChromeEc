/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "keyboard_scan.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

static void board_keyboard_init(void)
{
	CPRINTS("Setting vol-up to (3, 5)");
	set_vol_up_key(3, 5);
}
DECLARE_HOOK(HOOK_INIT, board_keyboard_init, HOOK_PRIO_DEFAULT);
