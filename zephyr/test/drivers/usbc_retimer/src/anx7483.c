/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/retimer/anx7483.h"
#include "emul/retimer/emul_anx7483.h"
#include "i2c.h"
#include "usb_mux.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/ztest.h>

#define ANX7483_EMUL EMUL_DT_GET(DT_NODELABEL(anx7483_emul))

#if 0
static struct usb_mux mux = {
	.i2c_port = I2C_PORT_NODELABEL(i2c3),
};
#endif

/* Helper functions to make tests clearer. */
static int anx7483_get_reg(int reg, uint8_t *val)
{
	return anx7483_emul_get_reg(ANX7483_EMUL, reg, val);
}

ZTEST_SUITE(anx7483, NULL, NULL, NULL, NULL, NULL);

ZTEST(anx7483, test_reset)
{
	uint8_t val;
	int rv;

	/* Verify that the reset value for all registers are correct. */
	rv = anx7483_get_reg(ANX7483_LFPS_TIMER_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_LFPS_TIMER_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_ANALOG_STATUS_CTRL_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_ANALOG_STATUS_CTRL_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_ENABLE_EQ_FLAT_SWING_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_ENABLE_EQ_FLAT_SWING_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_AUX_SNOOPING_CTRL_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_AUX_SNOOPING_CTRL_REG_DEFAULT);

	/* CFG0 */
	rv = anx7483_get_reg(ANX7483_UTX1_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX1_PORT_CFG0_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_UTX2_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX2_PORT_CFG0_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX1_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX1_PORT_CFG0_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX2_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX2_PORT_CFG0_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX1_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX1_PORT_CFG0_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX2_PORT_CFG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX2_PORT_CFG0_REG_DEFAULT);

	/* CFG1 */
	rv = anx7483_get_reg(ANX7483_UTX1_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX1_PORT_CFG1_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_UTX2_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX2_PORT_CFG1_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX1_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX1_PORT_CFG1_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX2_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX2_PORT_CFG1_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX1_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX1_PORT_CFG1_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX2_PORT_CFG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX2_PORT_CFG1_REG_DEFAULT);

	/* CFG2 */
	rv = anx7483_get_reg(ANX7483_UTX1_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX1_PORT_CFG2_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_UTX2_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX2_PORT_CFG2_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX1_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX1_PORT_CFG2_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX2_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX2_PORT_CFG2_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX1_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX1_PORT_CFG2_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX2_PORT_CFG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX2_PORT_CFG2_REG_DEFAULT);

	/* CFG3 */
	rv = anx7483_get_reg(ANX7483_UTX1_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX1_PORT_CFG3_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_UTX2_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX2_PORT_CFG3_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX1_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX1_PORT_CFG3_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX2_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX2_PORT_CFG3_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX1_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX1_PORT_CFG3_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX2_PORT_CFG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX2_PORT_CFG3_REG_DEFAULT);

	/* CFG4 */
	rv = anx7483_get_reg(ANX7483_UTX1_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX1_PORT_CFG4_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_UTX2_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_UTX2_PORT_CFG4_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX1_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX1_PORT_CFG4_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_URX2_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_URX2_PORT_CFG4_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX1_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX1_PORT_CFG4_REG_DEFAULT);

	rv = anx7483_get_reg(ANX7483_DRX2_PORT_CFG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, ANX7483_DRX2_PORT_CFG4_REG_DEFAULT);
}