/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/ztest.h>

#include "charger.h"
#include "driver/charger/rt9490.h"
#include "emul/emul_rt9490.h"

const struct emul *emul = EMUL_DT_GET(DT_NODELABEL(rt9490));
const int chgnum = CHARGER_SOLO;

ZTEST(rt9490_chg, current)
{
	struct {
		int reg;
		int expected; /* expected current in mA */
	} testdata[] = { { 0xF, 150 },	 { 0x10, 160 },	  { 0x64, 1000 },
			 { 0xC8, 2000 }, { 0x1F3, 4990 }, { 0x1F4, 5000 } };

	for (int i = 0; i < ARRAY_SIZE(testdata); i++) {
		int current = -1;

		zassert_equal(rt9490_drv.set_current(chgnum,
						     testdata[i].expected),
			      0, "case %d failed", i);
		zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_ICHG_CTRL),
			      testdata[i].reg >> 8, "case %d failed", i);
		zassert_equal(rt9490_emul_peek_reg(emul,
						   RT9490_REG_ICHG_CTRL + 1),
			      testdata[i].reg & 0xFF, "case %d failed", i);

		zassert_equal(rt9490_drv.get_current(chgnum, &current), 0,
			      "case %d failed", i);
		zassert_equal(testdata[i].expected, current, "case %d failed",
			      i);
	}

	/* special case: set_current(0) means 150mA */
	zassert_equal(rt9490_drv.set_current(chgnum, 0), 0, NULL);
	zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_ICHG_CTRL), 0,
		      NULL);
	zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_ICHG_CTRL + 1), 0xF,
		      NULL);

	/* values outside (150mA, 5000mA) are illegal */
	zassert_not_equal(rt9490_drv.set_current(chgnum, 140), 0, NULL);
	zassert_not_equal(rt9490_drv.set_current(chgnum, 5001), 0, NULL);
}

ZTEST(rt9490_chg, voltage)
{
	struct {
		int reg;
		int expected; /* expected voltage in mV */
	} testdata[] = { { 0x12C, 3000 },  { 0x12D, 3010 },  { 0x12E, 3020 },
			 { 0x1A4, 4200 },  { 0x348, 8400 },  { 0x4EC, 12600 },
			 { 0x690, 16800 }, { 0x757, 18790 }, { 0x758, 18800 } };

	for (int i = 0; i < ARRAY_SIZE(testdata); i++) {
		int voltage = -1;

		zassert_equal(rt9490_drv.set_voltage(chgnum,
						     testdata[i].expected),
			      0, "case %d failed", i);
		zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_VCHG_CTRL),
			      testdata[i].reg >> 8, "case %d failed", i);
		zassert_equal(rt9490_emul_peek_reg(emul,
						   RT9490_REG_VCHG_CTRL + 1),
			      testdata[i].reg & 0xFF, "case %d failed", i);

		zassert_equal(rt9490_drv.get_voltage(chgnum, &voltage), 0,
			      "case %d failed", i);
		zassert_equal(testdata[i].expected, voltage, "case %d failed",
			      i);
	}

	/* special case: set_voltage(0) means 3.0V */
	zassert_equal(rt9490_drv.set_voltage(chgnum, 0), 0, NULL);
	zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_VCHG_CTRL), 0x1,
		      NULL);
	zassert_equal(rt9490_emul_peek_reg(emul, RT9490_REG_VCHG_CTRL + 1),
		      0x2C, NULL);

	/* values outside (3V, 18.8V) are illegal */
	zassert_not_equal(rt9490_drv.set_voltage(chgnum, 2999), 0, NULL);
	zassert_not_equal(rt9490_drv.set_voltage(chgnum, 18801), 0, NULL);
}

static void reset_emul(void *fixture)
{
	rt9490_emul_reset_regs(emul);
	rt9490_drv.init(chgnum);
}

ZTEST_SUITE(rt9490_chg, NULL, NULL, reset_emul, NULL, NULL);
