/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "keyboard_customization.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{0x000e, 0x0016, 0x0015, 0x000d, 0x001c, 0x0076, 0x001a, 0x0000},
	{0x0005, 0x001e, 0x001d, 0xe01f, 0x001b, 0x0000, 0x0022, 0x0000},
	{0x0004, 0x0026, 0x0024, 0x000c, 0x0023, 0x0003, 0x0021, 0x0000},
	{0x002e, 0x0025, 0x002d, 0x002c, 0x002b, 0x0034, 0x002a, 0x0032},
	{0x0036, 0x003d, 0x003c, 0x0035, 0x003b, 0x0033, 0x003a, 0x0031},
	{0x0055, 0x003e, 0x0043, 0x005b, 0x0042, 0x000b, 0x0041, 0x0000},
	{0x000a, 0x0046, 0x0044, 0x0083, 0x004b, 0x0000, 0x0049, 0x0000},
	{0x004e, 0x0045, 0x004d, 0x0054, 0x004c, 0x0052, 0x0000, 0x004a},
	{0x0001, 0x0009, 0x0000, 0x0066, 0x005d, 0x0003, 0x005a, 0x0029},
	{0x002f, 0x0007, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xe074},
	{0x0000, 0x0078, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xe072},
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	{0x0000, 0x0064, 0x0000, 0x0000, 0x0000, 0xe075, 0x0000, 0xe06b},
	{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0011, 0x0000, 0xe011},
	{0x0000, 0x0000, 0x0000, 0x0012, 0x0000, 0x0000, 0x0059, 0x0000},
	{0x0014, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0xe014, 0x0000},
};

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

void board_keyboard_drive_col(int col)
{
	/* Drive all lines to high */
	if (col == KEYBOARD_COLUMN_NONE)
		gpio_set_level(GPIO_KBD_KSO2, 0);

	/* Set KBSOUT to zero to detect key-press */
	else if (col == KEYBOARD_COLUMN_ALL)
		gpio_set_level(GPIO_KBD_KSO2, 1);

	/* Drive one line for detection */
	else {
		if (col == 2)
			gpio_set_level(GPIO_KBD_KSO2, 1);
		else
			gpio_set_level(GPIO_KBD_KSO2, 0);
	}
}

#ifdef CONFIG_KEYBOARD_DEBUG
static char keycap_label[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{'`',    '1',    'q',    KLLI_TAB,
		'a',    KLLI_ESC,    'z',    KLLI_UNKNO},
	{KLLI_F1,    '2',    'w',    KLLI_SEARC,
		's',    KLLI_UNKNO,    'x',    KLLI_UNKNO},
	{KLLI_F2,    '3',    'e',    KLLI_F3,
		'd',    KLLI_F4,    'c',    KLLI_UNKNO},
	{'5',    '4',    'r',    't',
		'f',    'g',    'v',    'b'},
	{'6',    '7',    'u',    'y',
		'j',    'h',    'm',    'n'},
	{'=',    '8',    'i',    ']',
		'k',    KLLI_F6,    ',',    KLLI_UNKNO},
	{KLLI_F8,    '9',    'o',    KLLI_F7,
		'l',    KLLI_UNKNO,    '.',    KLLI_UNKNO},
	{'-',    '0',    'p',    '[',
		';',    '\'',    KLLI_UNKNO,    '/'},
	{KLLI_F9,    KLLI_F10,    KLLI_UNKNO,    KLLI_B_SPC,
		'\\',    KLLI_F5,    KLLI_ENTER,    KLLI_SPACE},
	{KLLI_UNKNO,    KLLI_F12,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_RIGHT},
	{KLLI_UNKNO,    KLLI_F11,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_DOWN},
	{KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO},
	{KLLI_F13,    KLLI_F14,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_UP,    KLLI_UNKNO,    KLLI_LEFT},
	{KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_L_ALT,    KLLI_UNKNO,    KLLI_R_ALT},
	{KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_L_SHT,
		KLLI_UNKNO,    KLLI_UNKNO,    KLLI_R_SHT,    KLLI_UNKNO},
	{KLLI_L_CTR,    KLLI_UNKNO,    KLLI_UNKNO,    KLLI_UNKNO,
		KLLI_UNKNO,    KLLI_UNKNO,    KLLI_R_CTR,    KLLI_UNKNO},
};

char get_keycap_label(uint8_t row, uint8_t col)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		return keycap_label[col][row];
	return KLLI_UNKNO;
}

void set_keycap_label(uint8_t row, uint8_t col, char val)
{
	if (col < KEYBOARD_COLS_MAX && row < KEYBOARD_ROWS)
		keycap_label[col][row] = val;
}
#endif
