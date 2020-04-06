/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Modular keyboard console commands for Chrome EC */

#ifndef __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H
#define __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H

/**
 * Simulate a keypress.
 *
 * @param row		Row of key
 * @param col		Column of key
 * @param pressed	Non-zero if pressed, zero if released
 */
void simulate_key(int row, int col, int pressed);

/**
 * Gets the simulated values for a given column.
 *
 * @param col Column of key
 * @return The row mask of this simulated column. Each BIT(x) represents row x.
 */
int get_simulated_col(int col);

#endif /* __CROS_EC_KEYBOARD_SCAN_CONSOLE_CMD_H */
