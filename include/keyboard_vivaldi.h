/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __KEYBOARD_VIVALDI_H__
#define __KEYBOARD_VIVALDI_H__

#define MAX_VIVALDI_KEYS	15

/* Scancodes for codeset 2 - Function keys */
#define SCANCODE_F1	0x05	/* Translates to 3B in codeset 1 */
#define SCANCODE_F2	0x06	/* Translates to 3C in codeset 1 */
#define SCANCODE_F3	0x04	/* Translates to 3D in codeset 1 */
#define SCANCODE_F4	0x0C	/* Translates to 3E in codeset 1 */
#define SCANCODE_F5	0x03	/* Translates to 3F in codeset 1 */
#define SCANCODE_F6	0x0B	/* Translates to 40 in codeset 1 */
#define SCANCODE_F7	0x83	/* Translates to 41 in codeset 1 */
#define SCANCODE_F8	0x0A	/* Translates to 42 in codeset 1 */
#define SCANCODE_F9	0x01	/* Translates to 43 in codeset 1 */
#define SCANCODE_F10	0x09	/* Translates to 44 in codeset 1 */
#define SCANCODE_F11	0x78	/* Translates to 57 in codeset 1 */
#define SCANCODE_F12	0x07	/* Translates to 58 in codeset 1 */
#define SCANCODE_F13	0x0F	/* Translates to 59 in codeset 1 */
#define SCANCODE_F14	0x17	/* Translates to 5A in codeset 1 */
#define SCANCODE_F15	0x1F	/* Translates to 5B in codeset 1 */

/* Scancodes for codeset 2 - Action keys */
#define SCANCODE_BACK		0xE038	/* Translates to E06A in codeset 1 */
#define SCANCODE_REFRESH	0xE020	/* Translates to E067 in codeset 1 */
#define SCANCODE_ZOOM		0xE01D	/* Translates to E011 in codeset 1 */
#define SCANCODE_SCALE		0xE024	/* Translates to E012 in codeset 1 */
#define SCANCODE_SNIP		0xE02D	/* Translates to E013 in codeset 1 */
#define SCANCODE_BRIGHTNESS_DOWN 0xE02C	/* Translates to E014 in codeset 1 */
#define SCANCODE_BRIGHTNESS_UP	0xE035	/* Translates to E015 in codeset 1 */
#define SCANCODE_PRIVACY_SCRN_TOGGLE 0xE03C /* Xlates to E016 in codeset 1 */
#define SCANCODE_VOL_MUTE	0xE023	/* Translates to E020 in codeset 1 */
#define SCANCODE_VOL_DOWN	0xE021	/* Translates to E02E in codeset 1 */
#define SCANCODE_VOL_UP		0xE032	/* Translates to E030 in codeset 1 */
#define SCANCODE_KBD_BKLIGHT_DOWN 0xE043 /* Translates to E017 in codeset 1 */
#define SCANCODE_KBD_BKLIGHT_UP	0xE044	/* Translates to E018 in codeset 1 */
#define SCANCODE_NEXT_TRACK	0xE04D	/* Translates to E019 in codeset 1 */
#define SCANCODE_PREV_TRACK	0xE015	/* Translates to E010 in codeset 1 */
#define SCANCODE_PLAY_PAUSE	0xE054	/* Translates to E01A in codeset 1 */

enum top_keys {
	T1 = 0,
	T2,
	T3,
	T4,
	T5,
	T6,
	T7,
	T8,
	T9,
	T10,
	T11,
	T12,
	T13,
	T14,
	T15
};

struct vivaldi_config {
	uint8_t num_top_row_keys;
	uint16_t scancodes[MAX_VIVALDI_KEYS];
};

void vivaldi_init(const struct vivaldi_config *vivaldi_config);

#endif /* __KEYBOARD_VIVALDI_H__ */
