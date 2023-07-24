/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "ec_commands.h"
#include "hooks.h"
#include "keyboard_customization.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static bool key_pad = FW_KB_NUMERIC_PAD_ABSENT;
static const struct ec_response_keybd_config craask_kb = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY,
};

static const struct ec_response_keybd_config craask_kb_w_kb_numpad = {
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_REFRESH,		/* T2 */
		TK_FULLSCREEN,		/* T3 */
		TK_OVERVIEW,		/* T4 */
		TK_SNAPSHOT,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.capabilities = KEYBD_CAP_SCRNLOCK_KEY | KEYBD_CAP_NUMERIC_KEYPAD,
};

__override const struct ec_response_keybd_config *
board_vivaldi_keybd_config(void)
{
	if (key_pad == FW_KB_NUMERIC_PAD_ABSENT)
		return &craask_kb;
	else
		return &craask_kb_w_kb_numpad;
}

static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ 0x0000, 0x0000, 0x0014, 0xe01f, 0x0061, 0xe007, 0x0000, 0x0000 },
	{ 0xe01f, 0x0076, 0x000d, 0x000e, 0x001c, 0x001a, 0x0016, 0x0015 },
	{ 0x0005, 0x000c, 0x0004, 0x0006, 0x0023, 0x0021, 0x0026, 0x0024 },
	{ 0x0032, 0x0034, 0x002c, 0x002e, 0x002b, 0x002a, 0x0025, 0x002d },
	{ 0x0009, 0x0083, 0x000b, 0x0003, 0x001b, 0x0022, 0x001e, 0x001d },
	{ 0x0051, 0x0000, 0x005b, 0x0000, 0x0042, 0x0041, 0x003e, 0x0043 },
	{ 0x0031, 0x0033, 0x0035, 0x0036, 0x003b, 0x003a, 0x003d, 0x003c },
	{ 0x0000, 0x0000, 0xe014, 0x0000, 0x0000, 0x0012, 0x0000, 0x0059 },
	{ 0x0055, 0x0052, 0x0054, 0x004e, 0x004c, 0x004a, 0x0045, 0x004d },
	{ 0x0000, 0x0001, 0x000a, 0x002f, 0x004b, 0x0049, 0x0046, 0x0044 },
	{ 0xe011, 0x0000, 0x006a, 0x0000, 0x005d, 0x0000, 0x0011, 0x0000 },
#ifndef CONFIG_KEYBOARD_KEYPAD
	{ 0x0000, 0x0066, 0x0000, 0x005d, 0x005a, 0x0029, 0xe072, 0xe075 },
	{ 0x0000, 0x0064, 0x0000, 0x0067, 0x0000, 0x0000, 0xe074, 0xe06b },
#else
	{ 0x0000, 0x0066, 0xe071, 0x005d, 0x005a, 0x0029, 0xe072, 0xe075 },
	{ 0xe06c, 0x0064, 0xe07d, 0x0067, 0xe069, 0xe07a, 0xe074, 0xe06b },
	{ 0xe04a, 0x007c, 0x007b, 0x0074, 0x0071, 0x0073, 0x006b, 0x0070 },
	{ 0x006c, 0x0075, 0x007d, 0x0079, 0x007a, 0x0072, 0x0069, 0xe05a },
#endif
};

void register_scancode_set2(uint16_t *scancode_set, size_t size)
{
	ASSERT(size == sizeof(scancode_set2));
	memcpy(scancode_set2, scancode_set, size);
}

uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		return scancode_set2[col][row];
	return 0;
}

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		scancode_set2[col][row] = val;
}

/*
 * Keyboard function decided by FW config.
 */
static void kb_init(void)
{
	int ret;
	uint32_t val;

	ret = cros_cbi_get_fw_config(FW_KB_NUMERIC_PAD, &val);

	if (ret != 0) {
		LOG_ERR("Error retrieving CBI FW_CONFIG field %d",
			FW_KB_NUMERIC_PAD);
	}

	if (val == FW_KB_NUMERIC_PAD_ABSENT) {
		/* Disable scanning KSO13 & 14 if keypad isn't present. */
		keyboard_raw_set_cols(KEYBOARD_COLS_NO_KEYPAD);
		key_pad = FW_KB_NUMERIC_PAD_ABSENT;
	} else {
		key_pad = FW_KB_NUMERIC_PAD_PRESENT;
		/* Setting scan mask KSO11, KSO12, KSO13 and KSO14 */
		keyscan_config.actual_key_mask[11] = 0xfe;
		keyscan_config.actual_key_mask[12] = 0xff;
		keyscan_config.actual_key_mask[13] = 0xff;
		keyscan_config.actual_key_mask[14] = 0xff;
	}
}
DECLARE_HOOK(HOOK_INIT, kb_init, HOOK_PRIO_POST_FIRST);

/*
 * We have total 30 pins for keyboard connecter {-1, -1} mean
 * the N/A pin that don't consider it and reserve index 0 area
 * that we don't have pin 0.
 */
const int keyboard_factory_scan_pins[][2] = {
	{ -1, -1 }, { 0, 5 },	{ 1, 1 }, { 1, 0 },   { 0, 6 },	  { 0, 7 },
	{ -1, -1 }, { -1, -1 }, { 1, 4 }, { 1, 3 },   { -1, -1 }, { 1, 6 },
	{ 1, 7 },   { 3, 1 },	{ 2, 0 }, { 1, 5 },   { 2, 6 },	  { 2, 7 },
	{ 2, 1 },   { 2, 4 },	{ 2, 5 }, { 1, 2 },   { 2, 3 },	  { 2, 2 },
	{ 3, 0 },   { -1, -1 }, { 0, 4 }, { -1, -1 }, { 8, 2 },	  { -1, -1 },
	{ -1, -1 },
};
const int keyboard_factory_scan_pins_used =
	ARRAY_SIZE(keyboard_factory_scan_pins);
