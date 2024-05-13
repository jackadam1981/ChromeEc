/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "hooks.h"
#include "keyboard_scan.h"

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, f ormat, ##args)

#define KB_SCAN_DISABLE_INVALID_KEYBOARD (1 << 7)

static void check_keyboard_connection(void)
{
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(wrg_kb_det_od))) {
		/* Invalid keyboard is connected */
		keyboard_scan_enable(0, KB_SCAN_DISABLE_INVALID_KEYBOARD);
	} else {
		/* Valid keyboard is connected */
		keyboard_scan_enable(1, KB_SCAN_DISABLE_INVALID_KEYBOARD);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, check_keyboard_connection, HOOK_PRIO_DEFAULT);
