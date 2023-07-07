/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "keyboard_protocol.h"

#include <stdio.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/input/input.h>

#define CROS_EC_KEYBOARD_NODE DT_CHOSEN(cros_ec_keyboard)

static void keyboard_input_cb(struct input_event *evt)
{
	static int row;
	static int col;
	static bool pressed;

	switch (evt->code) {
	case INPUT_ABS_X:
		col = evt->value;
		break;
	case INPUT_ABS_Y:
		row = evt->value;
		break;
	case INPUT_BTN_TOUCH:
		pressed = evt->value;
		break;
	}

	if (evt->sync) {
		printk("keyboard_state_changed %d %d %d\n", row, col, pressed);
		keyboard_state_changed(row, col, pressed);
	}
}
INPUT_LISTENER_CB_DEFINE(DEVICE_DT_GET(CROS_EC_KEYBOARD_NODE),
			 keyboard_input_cb);
