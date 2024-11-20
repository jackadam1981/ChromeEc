/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Keyboard configuration */

#ifndef __GCS_KBD_H
#define __GCS_KBD_H


/*
 * KEYBOARD_COLS_MAX has the build time column size. It's used to allocate
 * exact spaces for arrays. Actual keyboard scanning is done using
 * keyboard_cols, which holds a runtime column size.
 */
#undef KEYBOARD_COLS_MAX
#undef KEYBOARD_ROWS

#define KEYBOARD_COLS_MAX 16
#define KEYBOARD_ROWS 8

#define KEYBOARD_ROW_TO_MASK(r) (1 << (r))

/* Columns and masks for keys we particularly care about */
#define KEYBOARD_COL_DOWN 11
#define KEYBOARD_ROW_DOWN 5
#define KEYBOARD_MASK_DOWN KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_DOWN)

#define KEYBOARD_COL_ESC 1
#define KEYBOARD_ROW_ESC 3
#define KEYBOARD_MASK_ESC KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_ESC)

#define KEYBOARD_COL_KEY_H 5
#define KEYBOARD_ROW_KEY_H 3
#define KEYBOARD_MASK_KEY_H KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_KEY_H)

#define KEYBOARD_COL_KEY_R 4
#define KEYBOARD_ROW_KEY_R 0
#define KEYBOARD_MASK_KEY_R KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_KEY_R)

#define KEYBOARD_COL_LEFT_ALT 9
#define KEYBOARD_ROW_LEFT_ALT 3
#define KEYBOARD_MASK_LEFT_ALT KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_LEFT_ALT)

/* TODO */
#define KEYBOARD_COL_REFRESH 2
#define KEYBOARD_ROW_REFRESH 3
#define KEYBOARD_MASK_REFRESH KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_REFRESH)

#define KEYBOARD_COL_RIGHT_ALT 9
#define KEYBOARD_ROW_RIGHT_ALT 5
#define KEYBOARD_MASK_RIGHT_ALT KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_RIGHT_ALT)

#define KEYBOARD_DEFAULT_COL_VOL_UP 11
#define KEYBOARD_DEFAULT_ROW_VOL_UP 3

#define KEYBOARD_COL_LEFT_SHIFT 15
#define KEYBOARD_ROW_LEFT_SHIFT 1
#define KEYBOARD_MASK_LEFT_SHIFT KEYBOARD_ROW_TO_MASK(KEYBOARD_ROW_LEFT_SHIFT)

#endif /* __GCS_KBD_H */
