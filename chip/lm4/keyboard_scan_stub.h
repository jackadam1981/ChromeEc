/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Funciton needed by keyboard scanner module.
 */

#ifndef __CROS_EC_KEYBOARD_SCAN_STUB_H
#define __CROS_EC_KEYBOARD_SCAN_STUB_H

/* used for select_column() */
enum COLUMN_INDEX {
	COLUMN_ASSERT_ALL = -2,
	COLUMN_TRI_STATE_ALL = -1,
	/* 0 ~ 12 for the corresponding column */
};

/* Set keyboard scanning to enabled/disabled */
void set_scanning_enabled(int enabled);

/* Get keyboard scanning enabled/disabled */
int get_scanning_enabled(void);

/* Drive the specified column low; other columns are tristated */
void select_column(int col);

/* Clear current interrupt status */
uint32_t clear_matrix_interrupt_status(void);

/* Enable interrupt from keyboard matrix */
void enable_matrix_interrupt(void);

/* Disable interrupt from keyboard matrix */
void disable_matrix_interrupt(void);

/* Read raw row state */
int read_raw_row_state(void);

/* Configure keyboard matrix GPIO */
void configure_keyboard_gpio(void);

#endif  /* __CROS_EC_KEYBOARD_SCAN_STUB_H */
