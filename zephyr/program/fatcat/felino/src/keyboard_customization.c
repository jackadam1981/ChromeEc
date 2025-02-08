
/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "cros_cbi.h"
#include "gpio.h"
#include "hooks.h"
#include "keyboard_8042_sharedlib.h"
#include "keyboard_config.h"
#include "keyboard_protocol.h"
#include "keyboard_raw.h"

#include <zephyr/drivers/gpio.h>

LOG_MODULE_REGISTER(keyboard_init, LOG_LEVEL_ERR);

static uint16_t scancode_set2[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
/* KSO  ||  KSI0    KSI1    KSI2    KSI3    KSI4    KSI5    KSI6   KSI7*/
/*  0 */ { 0x0037, 0xe071, 0x0000, 0x0000, 0xe05a, 0x0000, 0x0000, 0x0071 },
/*  1 */ { 0x0076, 0x0005, 0x000e, 0x000d, 0x0027, 0x001c, 0x006a, 0x0016 },
/*  2 */ { 0x0079, 0x0012, 0x006c, 0x006b, 0x0059, 0x0000, 0x0069, 0x0000 },
/*  3 */ { 0xe06c, 0x0075, 0x0014, 0x0073, 0x0072, 0x0000, 0x0000, 0x0071 },
/*  4 */ { 0x0000, 0x0000, 0x0000, 0xe01f, 0x0000, 0x0000, 0x0000, 0x0000 },
/*  5 */ { 0x0004, 0x0006, 0x001e, 0x001d, 0x001b, 0x0022, 0x001a, 0x0015 },
/*  6 */ { 0xe069, 0x007d, 0x0000, 0x0074, 0x007a, 0x0011, 0xe011, 0x0000 },
/*  7 */ { 0x0003, 0x000c, 0x0025, 0x0024, 0x0023, 0x0021, 0x0000, 0x0026 },
/*  8 */ { 0x000b, 0x0036, 0x002e, 0x002d, 0x0034, 0x002b, 0x002a, 0x002c },
/*  9 */ { 0x0083, 0x003e, 0x003d, 0x0035, 0x0033, 0x0031, 0x0032, 0x003c },
/* 10 */ { 0x0001, 0x000a, 0x0043, 0x0042, 0x003a, 0x0041, 0x0029, 0x003b },
/* 11 */ { 0x007b, 0x0000, 0x0051, 0x005d, 0x0000, 0x0000, 0xe014, 0xe071 },
/* 12 */ { 0x0009, 0x0046, 0x0044, 0x004b, 0x0000, 0x0049, 0x0052, 0x004d },
/* 13 */ { 0xe01f, 0x0000, 0x0045, 0x0054, 0x0000, 0x004a, 0xe07d, 0x004e },
/* 14 */ { 0xe07a, 0x0000, 0x0055, 0x0061, 0xe075, 0x0000, 0xe06b, 0x005b },
/* 15 */ { 0x0000, 0x0000, 0x0066, 0x005a, 0x005d, 0x004c, 0xe072, 0xe074 },
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

#ifdef CONFIG_KEYBOARD_DEBUG
static uint8_t keycap_label[KEYBOARD_COLS_MAX][KEYBOARD_ROWS] = {
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_SEARC, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_ESC, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_F2, 'd', KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, 'r' },
	{ KLLI_F10, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, 's', KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, 'h', KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_L_SHT, KLLI_UNKNO, KLLI_R_SHT },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_R_ALT, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_ENTER,
	  KLLI_SPACE, KLLI_DOWN, KLLI_UP },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_RIGHT, KLLI_LEFT },
	{ KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO, KLLI_UNKNO,
	  KLLI_UNKNO, KLLI_L_ALT, KLLI_UNKNO },
	{ KLLI_UNKNO, KLLI_L_CTR, KLLI_UNKNO, KLLI_R_CTR, KLLI_UNKNO,
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
