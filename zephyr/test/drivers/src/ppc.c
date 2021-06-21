/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "emul/emul_syv682x.h"

#define SYV682X_ORD DT_DEP_ORD(DT_NODELABEL(syv682x_emul))

static void test_ppc_syv682x(void)
{
	struct i2c_emul *emul = syv682x_emul_get_ptr(SYV682X_ORD);
}

void test_suite_ppc(void)
{
	ztest_test_suite(ppc,
			 ztest_user_unit_test(test_ppc_syv682x));
	ztest_run_test_suite(ppc);
}
