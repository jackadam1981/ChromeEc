/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Keyboard mappings */

#ifndef __KEY_MAPPINGS_H
#define __KEY_MAPPINGS_H

#define KB_ROWS 10
#define KB_COLS 10

const struct key_mapping {
	uint8_t usb_id;
	uint8_t hap_row;
	uint8_t hap_col;
} mappings[KB_ROWS][KB_COLS] = {
	{
	/*  1A */ {0xe5, GPIO_HAP_ROW10, GPIO_HAP_COLG},/* right shift 3 */
	/*  1B */ {0xe5, GPIO_HAP_ROW10, GPIO_HAP_COLF},/* right shift 2 */
	/*  1C */ {0xe5, GPIO_HAP_ROW9, GPIO_HAP_COLH}, /* right shift 1 */
	/*  1D */ {0x2c, GPIO_HAP_ROW4, GPIO_HAP_COLI}, /* space 1 */
	/*  1E */ {0xe2, GPIO_HAP_ROW3, GPIO_HAP_COLI}, /* left alt 3 */
	/*  1F */ {0xe2, GPIO_HAP_ROW3, GPIO_HAP_COLH}, /* left alt 2 */
	/*  1G */ {0xe2, GPIO_HAP_ROW2, GPIO_HAP_COLI}, /* left alt 1 */
	/*  1H */ {0xe0, GPIO_HAP_ROW2, GPIO_HAP_COLH}, /* left ctrl 3 */
	/*  1I */ {0xe0, GPIO_HAP_ROW2, GPIO_HAP_COLG}, /* left ctrl 2 */
	/*  1J */ {0xe0, GPIO_HAP_ROW2, GPIO_HAP_COLF}, /* left ctrl 1 */
	},
	{
	/*  2A */ {0x52, 0, 0}, /* up arrow */
	/*  2B */ {0x50, 0, 0}, /* Left arrow */
	/*  2C */ {0xe4, GPIO_HAP_ROW9, GPIO_HAP_COLI}, /* right ctrl */
	/*  2D */ {0xe6, GPIO_HAP_ROW8, GPIO_HAP_COLI}, /* right alt */
	/*  2E */ {0x2c, GPIO_HAP_ROW8, GPIO_HAP_COLH}, /* space 7 */
	/*  2F */ {0x2c, GPIO_HAP_ROW7, GPIO_HAP_COLI}, /* space 6 */
	/*  2G */ {0x2c, GPIO_HAP_ROW6, GPIO_HAP_COLI}, /* space 5 */
	/*  2H */ {0x2c, GPIO_HAP_ROW6, GPIO_HAP_COLH}, /* space 4 */
	/*  2I */ {0x2c, GPIO_HAP_ROW5, GPIO_HAP_COLI}, /* space 3 */
	/*  2J */ {0x2c, GPIO_HAP_ROW5, GPIO_HAP_COLH}, /* space 2 */
	},
	{
	/*  3A */ { 0 }, /* not used */
	/*  3B */ { 0 }, /* not used */
	/*  3C */ { 0 }, /* not used */
	/*  3D */ { 0 }, /* not used */
	/*  3E */ { 0 }, /* not used */
	/*  3F */ { 0 }, /* not used */
	/*  3G */ { 0 }, /* not used */
	/*  3H */ { 0 }, /* not used */
	/*  3I */ {0x4f, 0, 0}, /* right arrow */
	/*  3J */ {0x51, 0, 0}, /* down arrow */
	},
	{
	/*  4A */ {0x38, GPIO_HAP_ROW9, GPIO_HAP_COLG}, /* / */
	/*  4B */ {0x37, GPIO_HAP_ROW8, GPIO_HAP_COLG}, /* 0 */
	/*  4C */ {0x36, GPIO_HAP_ROW8, GPIO_HAP_COLF}, /* " */
	/*  4D */ {0x10, GPIO_HAP_ROW7, GPIO_HAP_COLH}, /* m */
	/*  4E */ {0x11, GPIO_HAP_ROW6, GPIO_HAP_COLG}, /* n */
	/*  4F */ {0x05, GPIO_HAP_ROW6, GPIO_HAP_COLF}, /* b */
	/*  4G */ {0x19, GPIO_HAP_ROW4, GPIO_HAP_COLH}, /* v */
	/*  4H */ {0x06, GPIO_HAP_ROW4, GPIO_HAP_COLG}, /* c */
	/*  4I */ {0x1b, GPIO_HAP_ROW3, GPIO_HAP_COLG}, /* x */
	/*  4J */ {0x1d, GPIO_HAP_ROW2, GPIO_HAP_COLE}, /* z */
	},
	{
	/*  5A */ {0x28, GPIO_HAP_ROW10, GPIO_HAP_COLE}, /* enter 2 */
	/*  5B */ {0x28, GPIO_HAP_ROW9, GPIO_HAP_COLF}, /* enter 1 */
	/*  5C */ {0x34, GPIO_HAP_ROW9, GPIO_HAP_COLE}, /* ' */
	/*  5D */ {0x33, GPIO_HAP_ROW8, GPIO_HAP_COLE}, /* ; */
	/*  5E */ {0x0f, GPIO_HAP_ROW7, GPIO_HAP_COLG}, /* l */
	/*  5F */ {0x0e, GPIO_HAP_ROW7, GPIO_HAP_COLF}, /* k */
	/*  5G */ {0x0d, GPIO_HAP_ROW6, GPIO_HAP_COLE}, /* j */
	/*  5H */ {0xe1, GPIO_HAP_ROW1, GPIO_HAP_COLI}, /* left shift 3 */
	/*  5I */ {0xe1, GPIO_HAP_ROW1, GPIO_HAP_COLH}, /* left shift 2 */
	/*  5J */ {0xe1, GPIO_HAP_ROW1, GPIO_HAP_COLG}, /* left shift 1 */
	},
	{
	/*  6A */ {0x31, GPIO_HAP_ROW10, GPIO_HAP_COLD}, /* \ */
	/*  6B */ {0x30, GPIO_HAP_ROW9, GPIO_HAP_COLD}, /* ] */
	/*  6C */ {0x0b, GPIO_HAP_ROW5, GPIO_HAP_COLG}, /* h */
	/*  6D */ {0x0a, GPIO_HAP_ROW5, GPIO_HAP_COLF}, /* g */
	/*  6E */ {0x09, GPIO_HAP_ROW4, GPIO_HAP_COLF}, /* f */
	/*  6F */ {0x07, GPIO_HAP_ROW4, GPIO_HAP_COLE}, /* d */
	/*  6G */ {0x16, GPIO_HAP_ROW3, GPIO_HAP_COLF}, /* s */
	/*  6H */ {0x04, GPIO_HAP_ROW2, GPIO_HAP_COLD}, /* a */
	/*  6I */ {0xe3, GPIO_HAP_ROW1, GPIO_HAP_COLF}, /* search 2 */
	/*  6J */ {0xe3, GPIO_HAP_ROW1, GPIO_HAP_COLE}, /* search 1 */
	},
	{
	/*  7A */ {0x2f, GPIO_HAP_ROW9, GPIO_HAP_COLC}, /* [ */
	/*  7B */ {0x13, GPIO_HAP_ROW8, GPIO_HAP_COLD}, /* p */
	/*  7C */ {0x12, GPIO_HAP_ROW7, GPIO_HAP_COLE}, /* o */
	/*  7D */ {0x0c, GPIO_HAP_ROW7, GPIO_HAP_COLD}, /* i */
	/*  7E */ {0x18, GPIO_HAP_ROW6, GPIO_HAP_COLD}, /* u */
	/*  7F */ {0x1c, GPIO_HAP_ROW5, GPIO_HAP_COLE}, /* y */
	/*  7G */ {0x17, GPIO_HAP_ROW5, GPIO_HAP_COLD}, /* t */
	/*  7H */ {0x15, GPIO_HAP_ROW4, GPIO_HAP_COLD}, /* r */
	/*  7I */ {0x08, GPIO_HAP_ROW4, GPIO_HAP_COLC}, /* e */
	/*  7J */ {0x1a, GPIO_HAP_ROW3, GPIO_HAP_COLE}, /* w */
	},
	{
	/*  8A */ {0x2a, GPIO_HAP_ROW10, GPIO_HAP_COLC}, /* backspace 2 */
	/*  8B */ {0x2a, GPIO_HAP_ROW10, GPIO_HAP_COLB}, /* backspace 1 */
	/*  8C */ {0x2e, GPIO_HAP_ROW9, GPIO_HAP_COLB}, /* = */
	/*  8D */ {0x2d, GPIO_HAP_ROW8, GPIO_HAP_COLC}, /* - */
	/*  8E */ {0x27, GPIO_HAP_ROW8, GPIO_HAP_COLB}, /* 0 */
	/*  8F */ {0x26, GPIO_HAP_ROW7, GPIO_HAP_COLC}, /* 9 */
	/*  8G */ {0x25, GPIO_HAP_ROW7, GPIO_HAP_COLB}, /* 8 */
	/*  8H */ {0x14, GPIO_HAP_ROW2, GPIO_HAP_COLC}, /* q */
	/*  8I */ {0x2b, GPIO_HAP_ROW1, GPIO_HAP_COLD}, /* tab 2 */
	/*  8J */ {0x2b, GPIO_HAP_ROW1, GPIO_HAP_COLC}, /* tab 1 */
	},
	{
	/*  9A */ {0x66, GPIO_HAP_ROW10, GPIO_HAP_COLA}, /* power */
	/*  9B */ {0x43, GPIO_HAP_ROW9, GPIO_HAP_COLA}, /* volume up */
	/*  9C */ {0x24, GPIO_HAP_ROW6, GPIO_HAP_COLC}, /* 7 */
	/*  9D */ {0x23, GPIO_HAP_ROW5, GPIO_HAP_COLC}, /* 6 */
	/*  9E */ {0x22, GPIO_HAP_ROW5, GPIO_HAP_COLB}, /* 5 */
	/*  9F */ {0x21, GPIO_HAP_ROW4, GPIO_HAP_COLB}, /* 4 */
	/*  9G */ {0x20, GPIO_HAP_ROW3, GPIO_HAP_COLD}, /* 3 */
	/*  9H */ {0x1f, GPIO_HAP_ROW3, GPIO_HAP_COLC}, /* 2 */
	/*  9I */ {0x1e, GPIO_HAP_ROW2, GPIO_HAP_COLB}, /* 1 */
	/*  9J */ {0x35, GPIO_HAP_ROW1, GPIO_HAP_COLB}, /* ` */
	},
	{
	/* 10A */ {0x42, GPIO_HAP_ROW8, GPIO_HAP_COLA}, /* volume down */
	/* 10B */ {0x41, GPIO_HAP_ROW7, GPIO_HAP_COLA}, /* mute */
	/* 10C */ {0x40, GPIO_HAP_ROW6, GPIO_HAP_COLB}, /* brightness down */
	/* 10D */ {0x3f, GPIO_HAP_ROW6, GPIO_HAP_COLA}, /* brightness up */
	/* 10E */ {0x3e, GPIO_HAP_ROW5, GPIO_HAP_COLA}, /* extscreen */
	/* 10F */ {0x3d, GPIO_HAP_ROW4, GPIO_HAP_COLA}, /* maximize */
	/* 10G */ {0x3c, GPIO_HAP_ROW3, GPIO_HAP_COLB}, /* refresh */
	/* 10H */ {0x3b, GPIO_HAP_ROW3, GPIO_HAP_COLA}, /* -> */
	/* 10I */ {0x3a, GPIO_HAP_ROW2, GPIO_HAP_COLA}, /* <- */
	/* 10J */ {0x29, GPIO_HAP_ROW1, GPIO_HAP_COLA}, /* esc */
	},
};

#endif /* __KEY_MAPPINGS_H */
