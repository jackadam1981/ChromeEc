/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test lid switch.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "power_button.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

void run_test(void)
{
	test_reset();

	test_print_result();
}
