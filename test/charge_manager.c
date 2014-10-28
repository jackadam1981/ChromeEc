/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test charge manager module.
 */

#include "test_util.h"

void board_set_charge_limit(int charge_ma)
{
}

void board_set_active_charge_port(int charge_port)
{
}

void run_test(void)
{
	test_reset();

	test_print_result();
}
