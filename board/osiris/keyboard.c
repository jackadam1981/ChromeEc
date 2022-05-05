/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fw_config.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_scan.h"
#include "timer.h"


#define CPRINTF(format, args...) cprintf(CC_KEYBOARD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_KEYBOARD, format, ## args)

/* Keyboard scan setting */
__override struct keyboard_scan_config keyscan_config = {
	/* Increase from 50 us, because KSO_02 passes through the H1. */
	.output_settle_us = 80,
	/* Other values should be the same as the default configuration. */
	.debounce_down_us = 9 * MSEC,
	.debounce_up_us = 30 * MSEC,
	.scan_period_us = 3 * MSEC,
	.min_post_scan_delay_us = 1000,
	.poll_timeout_us = 100 * MSEC,
	.actual_key_mask = {
		0x14, 0xff, 0xff, 0xff, 0xff, 0xf5, 0xff,
		0xa4, 0xff, 0xfe, 0x55, 0xfa, 0xca  /* full set */
	},
};
/*
 * TODO(b/220800586): implement multiple keyboard matrix types
 */
static void board_keyboard_matrix_init(void)
{
	enum ec_cfg_keyboard_matrix_type mt_type =
					ec_cfg_keyboard_matrix_type();

	CPRINTS("FW_CONFIG: keyboard matrix type number is %d", mt_type);
	if (mt_type == KEYBOARD_MATRIX_RGB) {
		/* TODO(b/220800586): modify scancode set for new keyboard */
		/* TODO(b/220800586): modify actual_key_mask for new keyboard */
	}
}
DECLARE_HOOK(HOOK_INIT, board_keyboard_matrix_init, HOOK_PRIO_DEFAULT);
