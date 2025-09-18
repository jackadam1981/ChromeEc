/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This is a stub (no-op) keyboard protocol that can be used for testing when
 * an AP is not present. All key events are simply discarded. On Zephyr builds
 * using the upstream input subsystem, keyboard events can be seen in the
 * console by running `input dump on`.
 */

#include "ec_commands.h"

void keyboard_clear_buffer(void)
{
}

void keyboard_state_changed(int row, int col, int is_pressed)
{
}

void keyboard_update_button(enum keyboard_button_type button, int is_pressed)
{
}
