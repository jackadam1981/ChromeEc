/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * keyboard.h - Keyboard interface between EC core and EC Lib.
 */

#ifndef __CHIP_INTERFACE_KEYBOARD_H
#define __CHIP_INTERFACE_KEYBOARD_H

#include <stdint.h>
#include "cros_ec/include/ec_common.h"

#define MAX_KEYBOARD_MATRIX_ROWS 8
#define MAX_KEYBOARD_MATRIX_COLS 16

typedef void (*EcKeyboardCallback)(int col, int row, int is_pressed);

/* Registers a callback function to underlayer EC lib. So that any key state
 * change would notify the upper EC main code.
 *
 * Note that passing NULL removes any previously registered callback.
 */
EcError EcKeyboardRegister(EcKeyboardCallback cb);

/* Asks the underlayer EC lib what keys are pressed right now.
 * Returns a bit array of pressed key. Each byte represents a column. Thus
 * Return cols bytes long.
 */
EcError EcKeyboardGetState(uint8_t *bit_array);

#endif  /* __CHIP_INTERFACE_KEYBOARD_H */
