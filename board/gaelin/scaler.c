/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

//#include "common.h"
//#include "console.h"
//#include "ec_commands.h"
//#include "gpio.h"
//#include "i8042_protocol.h"
//#include "keyboard_8042.h"
#include "keyboard_protocol.h"
//#include "keyboard_scan.h"
//#include "lpc.h"
//#include "power_button.h"
//#include "system.h"
//#include "test_util.h"
//#include "timer.h"
//#include "util.h"

void scaler_test(void)
{
	//simulate_key(1, 1, 0);
	keyboard_state_changed(1, 4, 0); //row, col, is_pressed
}
