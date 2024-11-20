/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_customization.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

#include <zephyr/drivers/gpio.h>


LOG_MODULE_REGISTER(kbd, LOG_LEVEL_INF);

enum cust_keys {
	NONE = 0x0000,

	KEY_TAB = 0x000d,
	KEY_KL = 0x0058, /* TBD: Caps lock => Search */
	KEY_HPN = 0x004e,
	KEY_BKTK = 0x000e,
	KEY_EQVL = 0x0055,

	KEY_COL = 0x004c,
	KEY_APST = 0x0052, /* apostrophe */
	KEY_COM = 0x0041,
	KEY_DOT = 0x0049, //not working
	KEY_FSLSH = 0x004a, /* Frontslash */

	KEY_OBRC = 0x0054, /* Opening brace */
	KEY_CBRC = 0x005b, /* Closing brace */
	KEY_BSLSH = 0x005d, /* Backslash */


	KEY_0 = 0x0045,
	KEY_1 = 0x0016,
	KEY_2 = 0x001e,
	KEY_3 = 0x0026,
	KEY_4 = 0x0025,

	KEY_5 = 0x002e,
	KEY_6 = 0x0036,
	KEY_7 = 0x003d,
	KEY_8 = 0x003e,
	KEY_9 = 0x0046,

	KEY_A = 0x001c,
	KEY_B = 0x0032,
	KEY_C = 0x0021,
	KEY_D = 0x0023,
	KEY_E = 0x0024,

	KEY_F = 0x002b,
	KEY_G = 0x0034,
	KEY_H = 0x0033, //not working
	KEY_I = 0x0043,
	KEY_J = 0x003b,

	KEY_K = 0x0042,
	KEY_L = 0x004b,
	KEY_M = 0x003a,
	KEY_N = 0x0031,
	KEY_O = 0x0044, //not working

	KEY_P = 0x004d,
	KEY_Q = 0x0015,
	KEY_R = 0x002d,
	KEY_S = 0x001b,
	KEY_T = 0x002c,

	KEY_U = 0x003c,
	KEY_V = 0x002a,
	KEY_W = 0x001d,
	KEY_X = 0x0022,
	KEY_Y = 0x0035, //not working
	KEY_Z = 0x001a,

	KEY_ESC = 0x0076,
	KEY_MUTE = SCANCODE_VOLUME_MUTE,
	KEY_VOLDN = SCANCODE_VOLUME_DOWN,
	KEY_VOLUP = SCANCODE_VOLUME_UP,

	KEY_PGUP = 0xe07d, //not working
	KEY_PGDN = 0xe07a, //not working
	KEY_PGLF = 0xe06b, // not working
	KEY_PGRT = 0xe074, // not working

	KEY_LSHFT = 0x0012, // not working
	KEY_RSHFT = 0x0059, // not working
	KEY_ENTR = 0x005a,

	KEY_LCTRL = 0x0014, //not working
	KEY_FUNC = 0x0000, //TBD
	KEY_LALT = 0x0011,
	KEY_SPACE = 0x0029,
	KEY_RALT = 0xe011,
	KEY_RCTRL = 0xe014, /* Delete => Right control */
	KEY_BACKSP = 0x0000, //not working


	KEY_MAX,
};

/*
 * check the key 30 (row:3, col:0), and 128 (row:6, col:15).
 */
//row:col missing
static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	//0        1        2       3      4        5        6     7
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //0
	{ KEY_Q, KEY_TAB, KEY_A, KEY_ESC, KEY_Z, 0x0000, KEY_BKTK, KEY_1 }, //1
	{ KEY_W, KEY_KL, KEY_S, 0x0000, KEY_X, 0x0000, KEY_MUTE, KEY_2 }, //2
	{ KEY_E, KEY_VOLUP, KEY_D, 0x0000, KEY_C, 0x0000, KEY_VOLDN, KEY_3 }, //3
	{ KEY_R, KEY_T, KEY_F, KEY_G, KEY_V, KEY_B, KEY_5, KEY_4 }, //4
	{ KEY_U, 0x0000, KEY_J, 0x0000, KEY_M, KEY_N, KEY_6, KEY_7 }, //5
	{ KEY_I, KEY_CBRC, KEY_K, 0x0000, KEY_COM, 0x0000, KEY_EQVL, KEY_8 }, //6
	{ 0x0000, 0x0000, KEY_L, 0x0000, 0x0000, KEY_RCTRL, 0x0000, KEY_9 }, //7
	{ KEY_P, KEY_OBRC, KEY_COL, KEY_APST, 0x0000, KEY_FSLSH, KEY_HPN, KEY_0 }, //8
	{ 0x0000, 0x0000, 0x0000, KEY_LALT, 0x0000, KEY_RALT, 0x0000, 0x0000 }, //9
	{ 0x0000, 0x0000, KEY_BSLSH, 0x0000, KEY_ENTR, 0x0000, 0x0000, 0x0000 }, //10
	{ 0x0000, 0x0000, 0x0000, KEY_SPACE, 0x0000, KEY_PGDN, 0x0000, 0x0000 }, //11
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //12
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //13
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //14
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //15
#if 0
	{ 0x0000, 0x0000, 0x0000, 0xe01f, 0x0000, 0x0000, 0x0000, 0x0000 }, //0
	{ 0x0078, 0x0076, 0x000d, 0x000e, 0x001c, 0x0016, 0x001a, 0x003c }, //1
	{ 0x0005, 0x000c, 0x0004, 0x0006, 0x0023, 0x0041, 0x0026, 0x0043 }, //2
	{ 0x0032, 0x0034, 0x002c, 0x002e, 0x002b, 0x0049, 0x0025, 0x0044 }, //3
	{ 0x0009, 0x0083, 0x000b, 0x001b, 0x0003, 0x004a, 0x001e, 0x004d }, //4
	{ 0x0031, 0x0007, 0x005b, 0x000f, 0x0042, 0x0021, 0x003e, 0x0015* }, //5
	{ 0x0051, 0x0033, 0x0035, 0x004e, 0x003b, 0x0029, 0x0045, 0x001d* }, //6
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0012, 0x0000, 0x0059 }, //7
	{ 0x0055, 0x0052, 0x0054, 0x0036, 0x004c, 0x0022, 0x003d, 0x0024 }, //8
	{ 0xe06c, 0x0001, 0xe071, 0x002f, 0x004b, 0x002a, 0x0046, 0x002d }, //9
	{ 0xe011, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 }, //10
	{ 0x0017, 0x0066, 0x000a, 0x005d, 0x005a, 0x003a, 0xe072, 0xe075 }, //11
	{ 0x001f, 0x0064, 0xe07d, 0x0067, 0xe069, 0xe07a, 0xe074, 0xe06b }, //12
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0011, 0x0000 }, //13
	{ 0x0000, 0x0014, 0x0000, 0xe014, 0x0000, 0x0000, 0x0000, 0x0000 }, //14
	{ 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0027, 0x0000 }, //15
#if 0
	{ 0x0037, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 },
	{ 0x0000, 0x0000, 0x006a, 0x0000, 0x0000, 0x0000, 0x005d, 0x0061 },
#endif
#endif
};

uint16_t get_scancode_set2(uint8_t row, uint8_t col)
{
	uint16_t val = 0;
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		val = scancode_set2[col][row];
	
	LOG_ERR("****get val=0x%x, row=%d, col=%d", val, row, col);
	return val;
}

void set_scancode_set2(uint8_t row, uint8_t col, uint16_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		scancode_set2[col][row] = val;
	LOG_ERR("****set val=0x%x, row=%d, col=%d", val, row, col);
}

#define CPRINTF(format, args...) cprintf(CC_KEYSCAN, format, ##args)
#define CPRINTS(format, args...) cprints(CC_KEYSCAN, "KB " format, ##args)

static int cmd_set2(const struct shell *sh, size_t argc, char **argv)
{
	int err = 0, i, j;
	uint32_t row, col, val;

	row = shell_strtoul(argv[1], 0, &err);
	col = shell_strtoul(argv[2], 0, &err);
	val = shell_strtoul(argv[3], 0, &err);

	set_scancode_set2(row, col, val);

	for (i = 0; i < KEYBOARD_COLS_MAX; i++) {
		for (j = 0; j < KEYBOARD_ROWS; j++)
			CPRINTF(" %02x", scancode_set2[i][j]);

		CPRINTF("\n");
	}

	return 0;
}

SHELL_CMD_REGISTER(set2, NULL, "set row col val", cmd_set2);

#if 0
#ifdef CONFIG_KEYBOARD_DEBUG
static uint8_t keycap_label[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_SEARC, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_F11, KLLI_ESC, KLLI_TAB, '~', 'a', '1', 'z', 'u' },
	{ KLLI_F1, KLLI_F4, KLLI_F3, KLLI_F2, 'd', ',', '3', 'i' },
	{ 'b', 'g', 't', '5', 'f', '.', '4', 'o' },
	{ KLLI_F10, KLLI_F7, KLLI_F6, 's', KLLI_F5, '/', '2', 'p' },
	{ 'n', KLLI_F12, ']', KLLI_F13, 'k', 'c', '8', 'q' },
	{ KLLI_UNKNO, 'h', 'y', '-', 'j', KLLI_SPACE, '0', 'w' },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_L_SHT, KLLI_UNKNO, KLLI_R_SHT },
	{ '=', '\'', '[', '6', ';', 'x', '7', 'e' },
	{ KLLI_UNKNO, KLLI_F9, KLLI_UNKNO, KLLI_UNKNO, 'l', 'v', '9',
	  'r' }, /*delete at 2*/
	{ KLLI_R_ALT, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_F14, KLLI_B_SPC, KLLI_F8, KLLI_UNKNO, KLLI_ENTER, 'm', KLLI_DOWN,
	  KLLI_UP },
	{ KLLI_F15, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_RIGHT, KLLI_LEFT },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_L_ALT, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
};

uint8_t get_keycap_label(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		return keycap_label[col][row];
	return KLLI_UNKNO;
}

void set_keycap_label(uint8_t row, uint8_t col, uint8_t val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		keycap_label[col][row] = val;
}
#endif
#endif
