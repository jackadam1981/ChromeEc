/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gcs_kbd.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(kbd, LOG_LEVEL_INF);

enum cust_keys {
	KNONE = 0x0000,

	KEY_ESC = 0x0076, /* Escape */
	/*
	 * Function keys are replaced by vivaldi key config in the dts file.
	 * KEY_F1 = SCANCODE_F1,
	 * KEY_F2 = SCANCODE_F2,
	 * KEY_F3 = SCANCODE_F3,
	 * KEY_F4 = SCANCODE_F4,
	 * KEY_F5 = SCANCODE_F5,
	 * KEY_F6 = SCANCODE_F6,
	 * KEY_F7 = SCANCODE_F7,
	 * KEY_F8 = SCANCODE_F8,
	 * KEY_F9 = SCANCODE_F9,
	 * KEY_F10 = SCANCODE_F10,
	 * KEY_F11 = SCANCODE_F11,
	 * KEY_F12 = SCANCODE_F12,
	 */
	KEY_PWR = 0x0000, /* TODO: Power button */

	KEY_BKTK = 0x000e, /* Backtick */
	KEY_1 = SCANCODE_1,
	KEY_2 = SCANCODE_2,
	KEY_3 = SCANCODE_3,
	KEY_4 = SCANCODE_4,
	KEY_5 = SCANCODE_5,
	KEY_6 = SCANCODE_6,
	KEY_7 = SCANCODE_7,
	KEY_8 = SCANCODE_8,
	KEY_9 = 0x0046,
	KEY_0 = 0x0045,
	KEY_HPN = 0x004e, /* Hyphen */
	KEY_EQVL = 0x0055, /* Equal */
	KEY_BACKSP = 0x0066, /* Backspace */

	KEY_TAB = 0x000d,
	KEY_Q = 0x0015,
	KEY_W = 0x001d,
	KEY_E = 0x0024,
	KEY_R = 0x002d,
	KEY_T = 0x002c,
	KEY_Y = 0x0035,
	KEY_U = 0x003c,
	KEY_I = 0x0043,
	KEY_O = 0x0044,
	KEY_P = 0x004d,
	KEY_OBRC = 0x0054, /* Open brace */
	KEY_CBRC = 0x005b, /* Close brace */
	KEY_BSLSH = 0x005d, /* Backslash */

	KEY_KL = 0x0027, /* TODO: Caps lock */
	KEY_A = 0x001c,
	KEY_S = 0x001b,
	KEY_D = 0x0023,
	KEY_F = 0x002b,
	KEY_G = 0x0034,
	KEY_H = 0x0033,
	KEY_J = 0x003b,
	KEY_K = 0x0042,
	KEY_L = 0x004b,
	KEY_SCOL = 0x004c, /* Semicolon */
	KEY_APST = 0x0052, /* Apostrophe */
	KEY_ENTR = 0x005a, /* Enter */

	KEY_LSHFT = 0x0012, /* Left shift */
	KEY_Z = 0x001a,
	KEY_X = 0x0022,
	KEY_C = 0x0021,
	KEY_V = 0x002a,
	KEY_B = 0x0032,
	KEY_N = 0x0031,
	KEY_M = 0x003a,
	KEY_COM = 0x0041, /* Comma */
	KEY_DOT = 0x0049,
	KEY_FSLSH = 0x004a, /* Forwardslash */
	KEY_RSHFT = 0x0059, /* Right shift */

	KEY_LCTRL = 0x0014, /* Left control */
	KEY_FUNC = 0x0037, /* TODO: Function */
	KEY_GOOG = 0xe01f, /* TODO: Google assist */
	KEY_LALT = 0x0011, /* Left Alt */
	KEY_SPACE = 0x0029,
	KEY_RALT = 0xe011, /* Right Alt */
	KEY_RCTRL = 0xe014, /* Delete => Right control */
	KEY_LF = SCANCODE_LEFT, /* Left */
	KEY_UP = SCANCODE_UP, /* Up */
	KEY_DN = SCANCODE_DOWN, /* Down */
	KEY_RT = SCANCODE_RIGHT, /* Right */

	KEY_MAX,
};

/*
 * Scan codes
 */
static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	//0        1        2       3      4        5        6     7
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, KEY_LCTRL, 0x0000 }, //0
	{ KEY_Q, KEY_TAB, KEY_A, KEY_ESC, KEY_Z, 0x0000, KEY_BKTK, KEY_1 }, //1
	{ KEY_W, KEY_KL, KEY_S, 0x0000, KEY_X, 0x0000, 0x0000, KEY_2 }, //2
	{ KEY_E, 0x0000, KEY_D, 0x0000, KEY_C, 0x0000, 0x0000, KEY_3 }, //3
	{ KEY_R, KEY_T, KEY_F, KEY_G, KEY_V, KEY_B, KEY_5, KEY_4 }, //4
	{ KEY_U, KEY_Y, KEY_J, KEY_H, KEY_M, KEY_N, KEY_6, KEY_7 }, //5
	{ KEY_I, KEY_CBRC, KEY_K, 0x0000, KEY_COM, 0x0000, KEY_EQVL, KEY_8 }, //6
	{ KEY_O, 0x0000, KEY_L, 0x0000, KEY_DOT, KEY_RCTRL, 0x0000, KEY_9 }, //7
	{ KEY_P, KEY_OBRC, KEY_SCOL, KEY_APST, KEY_FUNC, KEY_FSLSH, KEY_HPN, KEY_0 }, //8
	{ 0x0000, 0x0000, 0x0000, KEY_LALT, 0x0000, KEY_RALT, 0x0000, 0x0000 }, //9
	{ 0x0000, KEY_BACKSP, KEY_BSLSH, 0x0000, KEY_ENTR, 0x0000, 0x0000, 0x0000 }, //10
	{ 0x0000, 0x0000, 0x0000, KEY_SPACE, 0x0000, KEY_DN, 0x0000, 0x0000 }, //11
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, KEY_RT, 0x0000, 0x0000 }, //12
	{ 0x0000, KEY_GOOG, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //13
	{ 0x0000, 0x0000, 0x0000, KEY_UP, 0x0000, KEY_LF, 0x0000, 0x0000 }, //14
	{ 0x0000, KEY_LSHFT, KEY_RSHFT, 0x0000, 0x0000, 0x0000, 0x0000, KEY_PWR }, //15
};
BUILD_ASSERT(ARRAY_SIZE(scancode_set2) == KEYBOARD_COLS_MAX);

uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	uint16_t val = 0;
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		val = scancode_set2[col][row];

	ccprintf("****get val=0x%x, row=%d, col=%d\n", val, row, col);
	return val;
}

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS) {
		scancode_set2[col][row] = val;
		ccprintf("****defset val=0x%x, row=%d, col=%d\n", val, row, col);
	}
}

void set_scancode_temp(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		scancode_set2[col][row] = val;
	ccprintf("****set val=0x%x, row=%d, col=%d\n", val, row, col);
}

/* TODO */
static void kb_init(void)
{
	int i;
	for (i = 0; i < KEYBOARD_COLS_MAX; i++) {
		//keyscan_config.actual_key_mask[i] = 0xff;
		ccprintf("***mask i%d=0x%x\n", i, keyscan_config.actual_key_mask[i]);
	}
}
DECLARE_HOOK(HOOK_INIT, kb_init, HOOK_PRIO_POST_FIRST);

static int cmd_set2(const struct shell *sh, size_t argc, char **argv)
{
	int err = 0, i, j;
	uint32_t row, col, val;

	row = shell_strtoul(argv[1], 0, &err);
	col = shell_strtoul(argv[2], 0, &err);
	val = shell_strtoul(argv[3], 0, &err);

	set_scancode_temp(row, col, val);

	for (i = 0; i < KEYBOARD_COLS_MAX; i++) {
		for (j = 0; j < KEYBOARD_ROWS; j++)
			ccprintf(" %02x", scancode_set2[i][j]);

		ccprintf("\n");
	}

	ccprintf("******mask\n");
	for (i = 0; i < KEYBOARD_COLS_MAX; i++)
		ccprintf("i%d=0x%x\n", i, keyscan_config.actual_key_mask[i]);

	return 0;
}
SHELL_CMD_REGISTER(set2, NULL, "set row col val", cmd_set2);
