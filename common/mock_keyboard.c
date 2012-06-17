/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Mock EC keyboard common code.
 */

#include "keyboard.h"
#include "uart.h"


void keyboard_state_changed(int row, int col, int is_pressed)
{
	/* Not implemented */
	return;
}


void keyboard_clear_underlying_buffer(void)
{
	/* Not implemented */
	return;
}


void keyboard_set_power_button(int pressed)
{
	uart_printf("Send power button %s keycode\n",
		    pressed ? "press" : "release");
	return;
}
