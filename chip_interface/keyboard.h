/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * keyboard.h - Keyboard interface between EC core and EC Lib.
 */

#ifndef __CHIP_INTERFACE_KEYBOARD_H
#define __CHIP_INTERFACE_KEYBOARD_H

#include <stdint.h>

#define MAX_KEYBOARD_MATRIX_ROWS 8
#define MAX_KEYBOARD_MATRIX_COLS 16


/* Regiters a callback function to underlayer EC lib. So that any key state
 * change would notify the upper EC main code.
 */
EcError CrKeyboardRegister(void (*cb)(in col, int row, int is_pressed));

/* Asks the underlayer EC lib what keys are pressed right now.
 * Returns a bit array of pressed key. Each byte represents a column. Thus
 * Return cols bytes long.
 */
EcError CrKeyboardGetState(uint8_t *bit_array);

#endif  /* __CHIP_INTERFACE_KEYBOARD_H */
