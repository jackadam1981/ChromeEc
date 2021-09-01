/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define LOG_LEVEL CONFIG_I2C_LOG_LEVEL
#include <logging/log.h>
LOG_MODULE_REGISTER(ppc);
#include <zephyr.h>
#include <ztest.h>
#include <ztest_assert.h>

#include "emul/emul_syv682x.h"

#include "stubs.h"
#include "syv682x.h"
#include "timer.h"
#include "usbc_ppc.h"

#define SYV682X_ORD DT_DEP_ORD(DT_NODELABEL(syv682x_emul))

static const int syv682x_port = 1;

static void test_ppc_syv682x_vbus_enable(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t reg;

	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB,
			SYV682X_CONTROL_1_PWR_ENB, "VBUS sourcing disabled");
	zassert_false(ppc_is_sourcing_vbus(syv682x_port),
			"PPC sourcing VBUS at beginning of test");

	zassert_ok(ppc_vbus_source_enable(syv682x_port, true),
			"VBUS enable failed");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_1_REG, &reg),
			"Reading CONTROL_1 failed");
	zassert_equal(reg & SYV682X_CONTROL_1_PWR_ENB, 0,
			"VBUS sourcing disabled");
	zassert_true(ppc_is_sourcing_vbus(syv682x_port),
			"PPC is not sourcing VBUS after VBUS enabled");
}

static void test_ppc_syv682x_interrupt(void)
{
	struct i2c_emul *emul = syv682x_emul_get(SYV682X_ORD);
	uint8_t status_reg;
	uint8_t control4_reg;

	syv682x_emul_set_reg(emul, SYV682X_STATUS_REG, SYV682X_STATUS_INT_MASK);
	syv682x_emul_set_reg(emul, SYV682X_CONTROL_4_REG,
			SYV682X_CONTROL_4_INT_MASK);
	syv682x_interrupt(syv682x_port);
	msleep(15);
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_STATUS_REG, &status_reg),
			"Couldn't read status register");
	zassert_ok(syv682x_emul_get_reg(emul, SYV682X_CONTROL_4_REG,
				&control4_reg),
			"Couldn't read status register");
	/*
	 * The alerting bits of these registers are clear on read. Checking that
	 * they are clear checks that the driver read them indirectly.
	 */
	zassert_equal(status_reg & SYV682X_STATUS_INT_MASK, 0x0,
			"Interrupt handled but status bits still set");
	zassert_equal(control4_reg & SYV682X_CONTROL_4_INT_MASK, 0x0,
			"Interrupt handled but control 4 bits still set");
}

static void test_ppc_syv682x(void)
{
	zassert_ok(ppc_init(syv682x_port), "PPC init failed");

	test_ppc_syv682x_vbus_enable();
	test_ppc_syv682x_interrupt();
}

void test_suite_ppc(void)
{
	ztest_test_suite(ppc,
			 ztest_user_unit_test(test_ppc_syv682x));
	ztest_run_test_suite(ppc);
}
