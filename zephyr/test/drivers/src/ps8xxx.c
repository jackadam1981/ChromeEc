/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_emul.h>

#include "common.h"
#include "ec_tasks.h"
#include "emul/emul_common_i2c.h"
#include "emul/emul_tcpci.h"
#include "emul/emul_ps8xxx.h"
#include "hooks.h"
#include "i2c.h"
#include "stubs.h"

#include "tcpm/tcpci.h"
#include "driver/tcpm/ps8xxx_public.h"

#define TCPCI_EMUL_LABEL DT_NODELABEL(tcpci_ps8xxx_emul)

/** Check TCPC register value */
static void check_tcpci_reg_f(const struct emul *emul, uint16_t exp_val,
			      int reg, int line)
{
	uint16_t reg_val;

	zassert_ok(tcpci_emul_get_reg(emul, reg, &reg_val),
		   "Failed tcpci_emul_get_reg(); line: %d", line);
	zassert_equal(exp_val, reg_val, "Expected 0x%x, got 0x%x; line: %d",
		      exp_val, reg_val, line);
}
#define check_tcpci_reg(emul, exp_val, reg)		\
	check_tcpci_reg_f(emul, exp_val, reg, __LINE__)

/** Test PS8xxx init */
static void test_ps8xxx_init(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(TCPCI_EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpci_emul_get_i2c_emul(emul);

	tcpci_emul_set_reg(emul, 0x82, 0x31);

	zassert_equal(EC_SUCCESS, ps8xxx_tcpm_drv.init(USBC_PORT_C1), NULL);
}

void test_suite_ps8xxx(void)
{
	ztest_test_suite(ps8805,
			 ztest_user_unit_test(test_ps8xxx_init));
	ztest_run_test_suite(ps8805);
}
