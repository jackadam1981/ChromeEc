/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros_cbi.h"
#include "ec_commands.h"
#include "hooks.h"
#include "keyboard_raw.h"
#include "keyboard_scan.h"

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
		TK_BRIGHTNESS_DOWN,		/* T2 */
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
	uint32_t val;

	cros_cbi_get_fw_config(FW_KB_NUMERIC_PAD, &val);

	if (val == FW_KB_NUMERIC_PAD_ABSENT)
		return &craask_kb;
	else
		return &craask_kb_w_kb_numpad;
}

static void board_update_no_keypad_by_fwconfig(void)
{
	uint32_t val;

	cros_cbi_get_fw_config(FW_KB_NUMERIC_PAD, &val);

	if (val == FW_KB_NUMERIC_PAD_ABSENT) {
		/* Disable scanning KSO13 & 14 if keypad isn't present. */
		keyboard_raw_set_cols(KEYBOARD_COLS_NO_KEYPAD);
	} else {
		/* Setting scan mask KSO11, KSO12, KSO13 and KSO14 */
		keyscan_config.actual_key_mask[11] = 0xfe;
		keyscan_config.actual_key_mask[12] = 0xff;
		keyscan_config.actual_key_mask[13] = 0xff;
		keyscan_config.actual_key_mask[14] = 0xff;
	}
}

/*
 * Keyboard function decided by FW config.
 */
static void kb_init(void)
{
	/* Support Keyboard Pad */
	board_update_no_keypad_by_fwconfig();
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
