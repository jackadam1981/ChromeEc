/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_scan.h"
#include "timer.h"

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
		0x1c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0x86, 0xff, 0xff, 0x55, 0xff, 0xff, 0xff, 0xff  /* full set */
	},
};

static uint16_t villager_scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{0x0000, 0x0000, 0x0014, 0xe01f, 0xe014, 0xe007, 0x0000, 0x0000},
	{0x001f, 0x0076, 0x001f, 0x000e, 0x001c, 0x003a, 0x000d, 0x0016},
	{0x001f, 0x000c, 0x0004, 0x0006, 0x0005, 0x001f, 0x0026, 0x002a},
	{0x0032, 0x0034, 0x002c, 0x002e, 0x002b, 0x0029, 0x0025, 0x002d},
	{0x001f, 0x0009, 0x0083, 0x000b, 0x0003, 0x004c, 0x001e, 0x001d},
	{0x0051, 0x0000, 0x005b, 0x0000, 0x0042, 0x0022, 0x003e, 0x0043},
	{0x0031, 0x0033, 0x0035, 0x0036, 0x003b, 0x001b, 0x003e, 0x003c},
	{0x0000, 0x0012, 0x0061, 0x0000, 0x0000, 0x0000, 0x0000, 0x0059},
	{0x0055, 0x0052, 0x0054, 0x004e, 0x004c, 0x0024, 0x0044, 0x004d},
	{0x0045, 0x0001, 0x000a, 0x002f, 0x0043, 0x0049, 0x0046, 0x001a},
	{0xe011, 0x0000, 0x006a, 0x0000, 0x005d, 0x0000, 0x0011, 0x0000},
	{0x001f, 0x005d, 0xe075, 0x001f, 0x005a, 0xe072, 0x004a, 0x0066},
	{0xe06b, 0xe074, 0x001f, 0x0067, 0x001f, 0x0064, 0x0015, 0x001f},
	{0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x0021},
	{0x0023, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f, 0x001f},
};

#ifdef CONFIG_KEYBOARD_FACTORY_TEST
/*
 * Map keyboard connector pins to EC GPIO pins for factory test.
 * Pins mapped to {-1, -1} are skipped.
 * The connector has 24 pins total, and there is no pin 0.
 */
const int keyboard_factory_scan_pins[][2] = {
	{-1, -1}, {0, 5}, {1, 1}, {1, 0}, {0, 6},
	{0, 7}, {-1, -1}, {-1, -1}, {1, 4}, {1, 3},
	{-1, -1}, {1, 6}, {1, 7}, {3, 1}, {2, 0},
	{1, 5}, {2, 6}, {2, 7}, {2, 1}, {2, 4},
	{2, 5}, {1, 2}, {2, 3}, {2, 2}, {3, 0},
	{-1, -1}, {-1, -1}, {-1, -1}, {-1, -1}, {-1, -1},
	{-1, -1},
};

const int keyboard_factory_scan_pins_used =
			ARRAY_SIZE(keyboard_factory_scan_pins);
#endif

static void keyboard_init(void)
{
	register_scancode_set2((uint16_t **) villager_scancode_set2,
				sizeof(villager_scancode_set2));
}
DECLARE_HOOK(HOOK_INIT, keyboard_init, HOOK_PRIO_DEFAULT);
