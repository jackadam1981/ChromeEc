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

/* TODO(b/220800586): need to verify on rgb keyboard */
uint8_t actual_key_mask_rgb[KEYBOARD_COLS_MAX] = {
		0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x86, 0xff, 0xff, 0x55, 0xff, 0xff, 0xff,
		0xff  /* full set */
};

/* TODO(b/220800586):  need to verify on rgb keyboard */
static void keyboard_init(void)
{
	int i;

	enum ec_cfg_keyboard_matrix_type mt_type =
					ec_cfg_keyboard_matrix_type();

	CPRINTS("FW_CONFIG: keyboard matrix type is %d", mt_type);

	if (mt_type == KEYBOARD_MATRIX_CROS)
		return;

	set_scancode_set2(0, 1, SCANCODE_F15);	  /* KSI0 KSO1  315 T15 */
	set_scancode_set2(2, 1, SCANCODE_F14);	  /* KSI2 KSO1  314 T14 */
	set_scancode_set2(5, 1, 0x003a);	  /* KSI5 KSO1  52  M */
	set_scancode_set2(6, 1, 0x000d);	  /* KSI6 KSO1  16  Tab */
	set_scancode_set2(7, 1, 0x0016);	  /* KSI7 KSO1  2   ! or 1 */
	set_scancode_set2(0, 2, 0x006c);	  /* KSI0 KSO2  91  Num 7 */
	set_scancode_set2(4, 2, 0x002a);	  /* KSI4 KSO2  49  V */
	set_scancode_set2(5, 3, 0x0029);	  /* KSI5 KSO3  61  Space */
	set_scancode_set2(0, 4, SCANCODE_F11);	  /* KSI0 KSO4  311 T11 */
	set_scancode_set2(1, 4, SCANCODE_F10);	  /* KSI1 KSO4  310 T10 */
	set_scancode_set2(2, 4, SCANCODE_F7);	  /* KSI2 KSO4  307 T07 */
	set_scancode_set2(3, 4, SCANCODE_F6);	  /* KSI3 KSO4  306 T06 */
	set_scancode_set2(4, 4, SCANCODE_F5);	  /* KSI4 KSO4  305 T05 */
	set_scancode_set2(5, 4, 0x0041);	  /* KSI5 KSO4  53  < or , */
	set_scancode_set2(5, 5, 0x0022);	  /* KSI5 KSO6  47  X */
	set_scancode_set2(5, 6, 0x001b);	  /* KSI5 KSO6  32  S */
	set_scancode_set2(0, 7, 0x0000);	  /* KSI0 KSO7  NA  NA */
	set_scancode_set2(1, 7, 0x0052);	  /* KSI1 KSO7  44  " or ' */
	set_scancode_set2(5, 7, 0x0000);	  /* KSI5 KSO7  NA  NA */
	set_scancode_set2(5, 8, 0x0024);	  /* KSI5 KSO8  19  E */
	set_scancode_set2(6, 8, 0x0044);	  /* KSI6 KSO8  25  O */
	set_scancode_set2(0, 9, 0x0045);	  /* KSI0 KSO9  11  ) or 9 */
	/* TODO(b/220800586): need to check lock key */
	set_scancode_set2(3, 9, 0x0000);	  /* KSI3 KSO9  59  Lock  */
	set_scancode_set2(7, 9, 0x001A);	  /* KSI7 KSO9  46  Z */
	set_scancode_set2(7, 10, 0x0000);	  /* KSI7 KSO10 NA  NA */
	set_scancode_set2(0, 11, 0xe07a);	  /* KSI0 KSO11 86  Page D */
	set_scancode_set2(1, 11, 0x005d);	  /* KSI1 KSO11 29  |  */
	set_scancode_set2(2, 11, SCANCODE_UP);	  /* KSI2 KSO11 83  Arrow U */
	set_scancode_set2(3, 11, 0x006b);	  /* KSI3 KSO11 92  Num 4 */
	set_scancode_set2(5, 11, SCANCODE_DOWN);  /* KSI5 KSO11 84  Arrow D */
	set_scancode_set2(6, 11, 0x004a);	  /* KSI6 KSO11 55  ? / */
	set_scancode_set2(7, 11, 0x0066);	 /* KSI7 KSO11 15  Backspace */
	set_scancode_set2(0, 12, SCANCODE_LEFT);  /* KSI0 KSO12 79  Arrow L */
	set_scancode_set2(1, 12, SCANCODE_RIGHT); /* KSI1 KSO12 89  Arrow R */
	set_scancode_set2(2, 12, 0xe069);	  /* KSI2 KSO12 81  END */
	set_scancode_set2(4, 12, 0xe0c6);	  /* KSI4 KSO12 80  Home */
	/* TODO(b/220800586): need to check 65 */
	set_scancode_set2(5, 12, 0x0000);	  /* KSI5 KSO12 65  NA */
	set_scancode_set2(6, 12, 0x0015);	  /* KSI6 KSO12 17  Q */
	set_scancode_set2(7, 12, 0xe07d);	  /* KSI7 KSO12 85  Page U */
	set_scancode_set2(0, 13, 0x0073);	  /* KSI0 KSO13 97  Num 5 */
	set_scancode_set2(5, 13, 0xe04a);	  /* KSI5 KSO13 95  Num / */
	set_scancode_set2(6, 13, 0x0070);	  /* KSI6 KSO13 99  Num 0 */
	set_scancode_set2(7, 13, 0x0021);	  /* KSI7 KSO13 48  C */
	set_scancode_set2(0, 14, 0x0023);	  /* KSI0 KSO14 33   D */
	set_scancode_set2(1, 14, 0xe05a);	/* KSI1 KSO14 108  Num Enter */
	set_scancode_set2(2, 14, 0x0075);	  /* KSI2 KSO14 96   Num 8 */
	set_scancode_set2(6, 14, 0x007d);	  /* KSI6 KSO14 101  Num 9 */
	set_scancode_set2(7, 14, 0x0069);	  /* KSI7 KSO14 93   Num 1 */

	/* update actual_key_mask */
	for (i = 0; i < KEYBOARD_COLS_MAX; i++)
		keyscan_config.actual_key_mask[i] = actual_key_mask_rgb[i];
}
DECLARE_HOOK(HOOK_INIT, keyboard_init, HOOK_PRIO_DEFAULT);
