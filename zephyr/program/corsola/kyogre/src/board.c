/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "keyboard_scan.h"

LOG_MODULE_REGISTER(board_init, LOG_LEVEL_ERR);

/* Vol-up key matrix */
#define KEYBOARD_COL_VOL_UP 9 /* EVT:COL 5, DVT:COL 9 */
#define KEYBOARD_ROW_VOL_UP 3 /* EVT:COL 3, DVT:COL 3*/

static void board_setup_init(void)
{
	/* Update vol up key */
	set_vol_up_key(KEYBOARD_ROW_VOL_UP, KEYBOARD_COL_VOL_UP);
}
DECLARE_HOOK(HOOK_INIT, board_setup_init, HOOK_PRIO_PRE_DEFAULT);
