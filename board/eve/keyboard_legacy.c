/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Eve board-specific keyboard configuration for legacy mode */

#include "board_config.h"
#include "chipset.h"
#include "keyboard_8042.h"
#include "keyboard_protocol.h"
#include "util.h"

/**
 * Special make codes to handle directly. Make code in all code sets should not
 * start with 0xf0 so we can use that to encode special values.
 */
#define MAKE_PAUSE	0xf000
#define MAKE_BREAK	0xf001
#define MAKE_DISP	0xf002
#define MAKE_DIM	0xf003
#define MAKE_BRIGHT	0xf004

/* Use SEARCH (before translate) as Fn key. */
static const struct makecode_entry makecode_fn_key = {
	.set1 = 0xe05b, .set2 = 0xe01f };

static const struct makecode_translate_entry legacy_mapping[] = {
	/* from[set1,set2], to[set1,set2] */
	{ {0xe058, 0xe007}, {0xe05b, 0xe020} },  /* ASSIST => SEARCH(Win) */
	{ {0x005d, 0x002f}, {0xe05d, 0xe02f} },  /* MENU => APP */
};

/* Alternate mapping when Fn is pressed. */
static const struct makecode_translate_entry legacy_fn_mapping[] = {
	/* from[set1,set2], to[set1,set2] */
	{{0x003b, 0x0005}, {0xe06a, 0xe038}},  /* F1 => Browser Back */
	{{0x003c, 0x0006}, {0xe067, 0xe020}},  /* F2 => Browser Refresh */
	{{0x003d, 0x0004}, {0x0057, 0x0078}},  /* F3 => Full Screen */
	{{0x003e, 0x000c}, {MAKE_DISP, MAKE_DISP}},  /* F4 => Switch Display */
	{{0x003f, 0x0003}, {MAKE_DIM, MAKE_DIM}},  /* F5 => Dim Screen */
	{{0x0040, 0x000b}, {MAKE_BRIGHT, MAKE_BRIGHT}},  /* F6 => Brighten */
	{{0x0041, 0x0083}, {0xe022, 0xe034}},  /* F7 => Play/Pause */
	{{0x0042, 0x000a}, {0xe020, 0xe023}},  /* F8 => Mute */
	{{0x0043, 0x0001}, {0xe02e, 0xe021}},  /* F9 => Vol Down */
	{{0x0044, 0x0009}, {0xe030, 0xe032}},  /* F10 => Vol Up */
	{{0x0038, 0x0011}, {0x003a, 0x0058}},  /* LAlt => Caps Lock */
	{{0x0002, 0x0016}, {0x003a, 0x0058}},  /* 1 => Caps Lock */
	{{0x0004, 0x0026}, {0xe037, 0xe07c}},  /* 2 => SysRq */
	{{0x0003, 0x001e}, {0x0004, 0x0026}},  /* 3 => PrtScrn */
	{{0x0005, 0x0025}, {0x0046, 0x007e}},  /* 4 => Scroll Lock */
	{{0x0006, 0x002e}, {MAKE_PAUSE, MAKE_PAUSE}},  /* 5 => Pause */
	{{0x0007, 0x0036}, {MAKE_BREAK, MAKE_BREAK}},  /* 6 => Break */
	{{0x0008, 0x003d}, {0xe052, 0xe070}},  /* 7 => Insert */
	{{0x0009, 0x003e}, {0xe053, 0xe071}},  /* 8 => Delete */
	{{0xe048, 0xe075}, {0xe049, 0xe07d}},  /* Up => Page Up */
	{{0xe050, 0xe072}, {0xe051, 0xe07a}},  /* Down => Page Down */
	{{0xe04b, 0xe06b}, {0xe047, 0xe06c}},  /* Left => Home */
	{{0xe04d, 0xe074}, {0xe04f, 0xe069}},  /* Right => End */
};

/* Indicate if the mapping is KEYBOARD_MAPPING_LEGACY. */
static int is_legacy_mapping;

/* Indicate if Fn key is already pressed. */
static int fn_pressed;

/* Override the board_init_keyboard_mapping in board.c */
void board_init_keyboard_mapping(int reset)
{
	if (reset && chipset_in_state(CHIPSET_STATE_SUSPEND))
		return;
	keyboard_select_mapping(KEYBOARD_MAPPING_DEFAULT);
}

/* Callback when the mapping is changed (keyboard_select_mapping called). */
void keyboard_board_mapping_changed(enum keyboard_mapping_type new_mapping)
{
	is_legacy_mapping = (new_mapping == KEYBOARD_MAPPING_LEGACY);
}

/* Translate legacy keys */
uint16_t keyboard_board_translate(uint16_t make_code, int8_t pressed,
				  enum scancode_set_list code_set)
{
	if (!is_legacy_mapping)
		return make_code;

	/* Fn must be processed because Fn makecode conflicts with Win. */
	if (makecode_match(make_code, code_set, &makecode_fn_key)) {
		fn_pressed = pressed;
		return 0;
	}

	make_code = makecode_translate(
			make_code, code_set, ARRAY_BEGIN(legacy_mapping),
			ARRAY_SIZE(legacy_mapping));
	if (!fn_pressed)
		return make_code;

	/* TODO(hungte): Handle  0xf0XX, which needs more translation. */
	return makecode_translate(
			make_code, code_set, ARRAY_BEGIN(legacy_fn_mapping),
			ARRAY_SIZE(legacy_fn_mapping));
}
