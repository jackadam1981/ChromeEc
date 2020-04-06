/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Modular keyboard console commands for Chrome EC */

#ifndef __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H
#define __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H

/* All deoendent header files needed for keyboard console */
#include "console.h"
#include "keyboard_config.h"
#include "stdbool.h"
#include "util.h"

/**
 * Gets the number of columns in use for the keyboard
 *
 * @return The number of columns in use for the keyboard
 */
uint8_t get_num_col(void);

/**
 * Simulate a keypress.
 *
 * @param row		Row of key
 * @param col		Column of key
 * @param pressed	Non-zero if pressed, zero if released
 */
void simulate_key(uint8_t row, uint8_t col, bool pressed);

/**
 * Gets the simulated values for a given column.
 *
 * @param col Column of key
 * @return The row mask of this simulated column. Each BIT(x) represents row x.
 */
uint8_t get_simulated_col(uint8_t col);

#endif /* __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H */
