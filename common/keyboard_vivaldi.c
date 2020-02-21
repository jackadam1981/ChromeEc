/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Vivali Keyboard code for Chrome EC */

#include "keyboard_8042_sharedlib.h"
#include "keyboard_scan.h"
<<<<<<< HEAD   (765a88 nightfury : remove ambient thermal sensor)
#include "keyboard_vivaldi.h"

/*
 * Row Column info for Top row keys T1 - T15
 * Ref: https://drive.google.com/corp/drive/folders/17UtVQ-AixnlQuicRPTp8t46HE-sT522E
 */
static const struct key {
	uint8_t row;
	uint8_t col;
} vivaldi[MAX_VIVALDI_KEYS] = {
	[T1] = {.row = 0, .col = 2},
	[T2] = {.row = 3, .col = 2},
	[T3] = {.row = 2, .col = 2},
	[T4] = {.row = 1, .col = 2},
	[T5] = {.row = 3, .col = 4},
	[T6] = {.row = 2, .col = 4},
	[T7] = {.row = 1, .col = 4},
	[T8] = {.row = 2, .col = 9},
	[T9] = {.row = 1, .col = 9},
	[T10] = {.row = 0, .col = 4},
	[T11] = {.row = 0, .col = 1},
	[T12] = {.row = 1, .col = 5},
	[T13] = {.row = 3, .col = 5},
	[T14] = {.row = 0, .col = 9},
	[T15] = {.row = 0, .col = 11},
};

void vivaldi_init(const struct vivaldi_config *keybd)
{
	uint8_t row, col, *mask;
	int key;

	cprints(CC_KEYBOARD, "VIVALDI: Num top row keys = %u",
		keybd->num_top_row_keys);

	if (keybd->num_top_row_keys > MAX_VIVALDI_KEYS ||
	    keybd->num_top_row_keys < 10)
		cprints(CC_KEYBOARD,
			"BAD VIVALDI CONFIG! Some keys may not work");

	for (key = T1; key < MAX_VIVALDI_KEYS; key++) {

		row = vivaldi[key].row;
		col = vivaldi[key].col;
		mask = keyscan_config.actual_key_mask + col;

		if (key < keybd->num_top_row_keys && keybd->scancodes[key]) {

			/* Enable the mask */
			*mask |= (1 << row);

			/* Populate the scancode */
			scancode_set2[col][row] = keybd->scancodes[key];
			cprints(CC_KEYBOARD,
				"VIVALDI key-%u (r-%u, c-%u) = scancode-%X",
				key, row, col, keybd->scancodes[key]);
		} else {
			/* Disable the mask */
			*mask &= ~(1 << row);
		}
	}
}
=======
#include "ec_commands.h"
#include <host_command.h>
#include <util.h>
#include <hooks.h>

/*
 * Row Column info for Top row keys T1 - T15
 * Ref: https://drive.google.com/corp/drive/folders/17UtVQ-AixnlQuicRPTp8t46HE-sT522E
 */
static const struct key {
	uint8_t row;
	uint8_t col;
} vivaldi[MAX_TOP_ROW_KEYS] = {
	{.row = 0, .col = 2},	/* T1 */
	{.row = 3, .col = 2},	/* T2 */
	{.row = 2, .col = 2},	/* T3 */
	{.row = 1, .col = 2},	/* T4 */
	{.row = 3, .col = 4},	/* T5 */
	{.row = 2, .col = 4},	/* T6 */
	{.row = 1, .col = 4},	/* T7 */
	{.row = 2, .col = 9},	/* T8 */
	{.row = 1, .col = 9},	/* T9 */
	{.row = 0, .col = 4},	/* T10 */
	{.row = 0, .col = 1},	/* T11 */
	{.row = 1, .col = 5},	/* T12 */
	{.row = 3, .col = 5},	/* T13 */
	{.row = 0, .col = 9},	/* T14 */
	{.row = 0, .col = 11},	/* T15 */
};

/* Scancodes for top row action keys */
static const uint16_t action_scancodes[] = {
	[TK_BACK] = SCANCODE_BACK,
	[TK_FORWARD] = SCANCODE_FORWARD,
	[TK_REFRESH] = SCANCODE_REFRESH,
	[TK_FULLSCREEN] = SCANCODE_FULLSCREEN,
	[TK_OVERVIEW] = SCANCODE_OVERVIEW,
	[TK_VOL_MUTE] = SCANCODE_VOLUME_MUTE,
	[TK_VOL_DOWN] = SCANCODE_VOLUME_DOWN,
	[TK_VOL_UP] = SCANCODE_VOLUME_UP,
	[TK_PLAY_PAUSE] = SCANCODE_PLAY_PAUSE,
	[TK_NEXT_TRACK] = SCANCODE_NEXT_TRACK,
	[TK_PREV_TRACK] = SCANCODE_PREV_TRACK,
	[TK_SNIP] = SCANCODE_SNIP,
	[TK_BRIGHTNESS_DOWN] = SCANCODE_BRIGHTNESS_DOWN,
	[TK_BRIGHTNESS_UP] = SCANCODE_BRIGHTNESS_UP,
	[TK_KBD_BKLIGHT_DOWN] = SCANCODE_KBD_BKLIGHT_DOWN,
	[TK_KBD_BKLIGHT_UP] = SCANCODE_KBD_BKLIGHT_UP,
	[TK_PRIVACY_SCRN_TOGGLE] = SCANCODE_PRIVACY_SCRN_TOGGLE,
};

static struct top_row_layout default_top_row = {
	/* Default Chromeos keyboard top row layout */
	.num_top_row_keys = 10,
	.action_keys = {
		TK_BACK,		/* T1 */
		TK_FORWARD,		/* T2 */
		TK_REFRESH,		/* T3 */
		TK_FULLSCREEN,		/* T4 */
		TK_OVERVIEW,		/* T5 */
		TK_BRIGHTNESS_DOWN,	/* T6 */
		TK_BRIGHTNESS_UP,	/* T7 */
		TK_VOL_MUTE,		/* T8 */
		TK_VOL_DOWN,		/* T9 */
		TK_VOL_UP,		/* T10 */
	},
	.can_send_function_keys = false,
};

struct top_row_layout *vivaldi_top_row;

static enum ec_status get_vivaldi_top_row(struct host_cmd_handler_args *args)
{
	struct ec_response_top_row_layout *resp = args->response;
	struct top_row_layout *top_row = &resp->top_row;

	if (vivaldi_top_row) {
		memcpy(top_row, vivaldi_top_row, sizeof(*top_row));
		args->response_size = sizeof(*resp);
		return EC_RES_SUCCESS;
	}
	return EC_RES_ERROR;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_KEYBD_TOP_ROW_LAYOUT, get_vivaldi_top_row,
		     EC_VER_MASK(0));

__overridable void board_set_vivaldi_top_row(void)
{
	vivaldi_top_row = &default_top_row;
}

static void vivaldi_init(void)
{
	uint8_t row, col, *mask;
	uint8_t i;
	enum action_key key;

	/* Allow the boards to change the top row layout */
	board_set_vivaldi_top_row();

	if (!vivaldi_top_row || !vivaldi_top_row->num_top_row_keys) {
		cprints(CC_KEYBOARD, "VIVALDI keybd disabled on board request");
		return;
	}

	cprints(CC_KEYBOARD, "VIVALDI: Num top row keys = %u",
		vivaldi_top_row->num_top_row_keys);

	if (vivaldi_top_row->num_top_row_keys > MAX_TOP_ROW_KEYS ||
	    vivaldi_top_row->num_top_row_keys < MIN_TOP_ROW_KEYS) {
		cprints(CC_KEYBOARD, "VIVALDI: Error! Bad Config, disabling");
		return;
	}

	for (i = 0; i < MAX_TOP_ROW_KEYS; i++) {

		row = vivaldi[i].row;
		col = vivaldi[i].col;
		key = vivaldi_top_row->action_keys[i];
		mask = keyscan_config.actual_key_mask + col;

		if (i < vivaldi_top_row->num_top_row_keys && key) {

			/* Enable the mask */
			*mask |= (1 << row);

			/* Populate the scancode */
			scancode_set2[col][row] = action_scancodes[key];
			cprints(CC_KEYBOARD,
				"VIVALDI key-%u (r-%u, c-%u) = scancode-%X",
				i, row, col, action_scancodes[key]);
		} else {
			/* Disable the mask */
			*mask &= ~(1 << row);
		}
	}
}
DECLARE_HOOK(HOOK_INIT, vivaldi_init, HOOK_PRIO_DEFAULT);
>>>>>>> CHANGE (6c217a common/keyboard: Start sending action codes by default (aka )
