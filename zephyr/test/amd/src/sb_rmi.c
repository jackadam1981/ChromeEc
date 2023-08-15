/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "emul/emul_amd_sb_rmi.h"
#include "driver/sb_rmi.h"

#include <zephyr/devicetree.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#define AMD_SB_RMI_EMUL EMUL_DT_GET(DT_NODELABEL(sb_rmi_emul))

int sb_rmi_write(const int reg, int data);
int sb_rmi_read(const int reg, int *data);
int sb_rmi_assert_interrupt(bool assert);

/* Helper functions to make tests clearer. */
static int amd_sb_rmi_emul_test_get_reg(int reg, uint8_t *val)
{
	return amd_sb_rmi_emul_get_reg(AMD_SB_RMI_EMUL, reg, val);
}

static int amd_sb_rmi_emul_test_set_reg(int reg, uint8_t val)
{
	return amd_sb_rmi_emul_set_reg(AMD_SB_RMI_EMUL, reg, val);
}

ZTEST_SUITE(sb_rmi, NULL, NULL, NULL, NULL, NULL);

/* Verify that the reset values for all registers are correct. */
ZTEST(sb_rmi, test_emul_reset)
{
	uint8_t val;
	int rv;

	/* All currently implemented registers are zero at reset. */
	/* In-bound registers. */
	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG5_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG6_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_IN_BND_MSG7_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	/* Out-bound registers. */
	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG0_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG1_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG2_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG3_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG4_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG5_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG6_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_OUT_BND_MSG7_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	/* Status and control registers. */
	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_STATUS_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);

	rv = amd_sb_rmi_emul_test_get_reg(SB_RMI_SW_INTR_REG, &val);
	zexpect_ok(rv);
	zexpect_equal(val, 0);
}

/* Validate that accessing the emulator's registers through I2C works. */
ZTEST(sb_rmi, test_emul_registers_rw)
{
	int rv;
	int val;

	/* Ensure that reading/writing a non-existent register fails. */
	rv = sb_rmi_read(0xff, &val);
	zexpect_not_equal(rv, 0);

	rv = sb_rmi_write(0xff, 0xff);
	zexpect_not_equal(rv, 0);
}

/* Validates that writing to a reserved register returns an error. */
ZTEST(sb_rmi, test_emul_reserved)
{
	int rv;

	/* In-bound registers. */
	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_IN_BND_MSG5_REG,
					  SB_RMI_IN_BND_MSG5_REG_RESERVED);
	zexpect_not_equal(rv, 0);

	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_IN_BND_MSG6_REG,
					  SB_RMI_IN_BND_MSG6_REG_RESERVED);
	zexpect_not_equal(rv, 0);

	/* Out-bound registers. */
	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_OUT_BND_MSG5_REG,
					  SB_RMI_OUT_BND_MSG5_REG_RESERVED);
	zexpect_not_equal(rv, 0);

	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_OUT_BND_MSG6_REG,
					  SB_RMI_OUT_BND_MSG6_REG_RESERVED);
	zexpect_not_equal(rv, 0);

	/* Status and control registers. */
	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_STATUS_REG,
					  SB_RMI_STATUS_REG_RESERVED);
	zexpect_not_equal(rv, 0);

	rv = amd_sb_rmi_emul_test_set_reg(SB_RMI_SW_INTR_REG,
					  SB_RMI_SW_INTR_REG_RESERVED);
	zexpect_not_equal(rv, 0);
}