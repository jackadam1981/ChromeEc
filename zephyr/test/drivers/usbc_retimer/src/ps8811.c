/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/retimer/ps8811.h"
#include "emul/retimer/emul_ps8811.h"
#include "i2c.h"
#include "usb_mux.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/ztest.h>

#define PS811_EMUL EMUL_DT_GET(DT_NODELABEL(ps8811_emul))

static struct usb_mux mux = {
	.i2c_port = I2C_PORT_NODELABEL(i2c3),
	.i2c_addr_flags = PS8811_I2C_ADDR_FLAGS3,
};

static void ps8811_before(void *fixture)
{
	ARG_UNUSED(fixture);
	ps8811_emul_reset(PS811_EMUL);
}

ZTEST_SUITE(ps8811, NULL, NULL, ps8811_before, NULL, NULL);

/* Helper functions to make tests clearer. */
static int ps8811_read0(int reg, int *val)
{
	return ps8811_i2c_read(&mux, PS8811_REG_PAGE0, reg, val);
}

static int ps8811_write0(int reg, int val)
{
	return ps8811_i2c_write(&mux, PS8811_REG_PAGE0, reg, val);
}

static int ps8811_update0(int reg, uint8_t mask, uint8_t val)
{
	return ps8811_i2c_field_update(&mux, PS8811_REG_PAGE0, reg, mask, val);
}

static int ps8811_read1(int reg, int *val)
{
	return ps8811_i2c_read(&mux, PS8811_REG_PAGE1, reg, val);
}

static int ps8811_write1(int reg, int val)
{
	return ps8811_i2c_write(&mux, PS8811_REG_PAGE1, reg, val);
}

static int ps8811_update1(int reg, uint8_t mask, uint8_t val)
{
	return ps8811_i2c_field_update(&mux, PS8811_REG_PAGE1, reg, mask, val);
}

static int ps8811_get_reg1(int reg, uint8_t *val)
{
	return ps8811_emul_get_reg1(PS811_EMUL, reg, val);
}

static int ps8811_set_reg1(int reg, uint8_t val)
{
	return ps8811_emul_set_reg1(PS811_EMUL, reg, val);
}

ZTEST(ps8811, test_reset)
{
	uint8_t val;
	int rv;

	/* Verify that the reset value for all registers are correct. */
	rv = ps8811_get_reg1(PS8811_REG1_USB_AEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_AEQ_LEVEL_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_ADE_CONFIG, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_ADE_CONFIG_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_BEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_BEQ_LEVEL_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_BDE_CONFIG, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_BDE_CONFIG_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_A_SWING, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_CHAN_A_SWING_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_50OHM_ADJUST_CHAN_B, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_50OHM_ADJUST_CHAN_B_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_SWING, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_CHAN_B_SWING_DEFAULT);

	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_CHAN_B_DE_PS_LSB_DEFAULT);

	ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB, &val);
	zassert_ok(rv);
	zassert_equal(val, PS8811_REG1_USB_CHAN_B_DE_PS_MSB_DEFAULT);
}

ZTEST(ps8811, test_p0_registers)
{
	int rv;
	int val;

	/*
	 * P0 registers aren't currently implemented,
	 * ensure access results in an error.
	 */
	rv = ps8811_write0(PS8811_REG0_A_STATUS, 0);
	zassert_not_equal(rv, EC_SUCCESS);
	rv = ps8811_read0(PS8811_REG0_A_STATUS, &val);
	zassert_not_equal(rv, EC_SUCCESS);
	rv = ps8811_update0(PS8811_REG0_A_STATUS, 0xff, 0xff);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write0(PS8811_REG0_B_STATUS, 0);
	zassert_not_equal(rv, EC_SUCCESS);
	rv = ps8811_read0(PS8811_REG0_B_STATUS, &val);
	zassert_not_equal(rv, EC_SUCCESS);
	rv = ps8811_update0(PS8811_REG0_B_STATUS, 0xff, 0xff);
	zassert_not_equal(rv, EC_SUCCESS);
}

ZTEST(ps8811, test_p1_rw)
{
	int val;
	int rv;

	/*
	 * Verify that writing/reading all our registers through I2C works. But
	 * don't attempt to verify errors when writing reserved bits.
	 */
	rv = ps8811_write1(PS8811_REG1_USB_AEQ_LEVEL, 0xff);
	zassert_ok(rv, EC_SUCCESS);
	rv = ps8811_read1(PS8811_REG1_USB_AEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xff);

	rv = ps8811_write1(PS8811_REG1_USB_ADE_CONFIG, 0xff);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_ADE_CONFIG, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xff);

	rv = ps8811_write1(PS8811_REG1_USB_BEQ_LEVEL, 0xff);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_BEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xff);

	const uint8_t bde_expected =
		(uint8_t)(~PS8811_REG1_USB_BDE_CONFIG_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_USB_BDE_CONFIG, bde_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_BDE_CONFIG, &val);
	zassert_ok(rv);
	zassert_equal(val, bde_expected);

	const uint8_t a_swing_expected =
		(uint8_t)(~PS8811_REG1_USB_CHAN_A_SWING_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_USB_CHAN_A_SWING, a_swing_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_CHAN_A_SWING, &val);
	zassert_ok(rv);
	zassert_equal(val, a_swing_expected);

	const uint8_t adjust_b_50ohm_expected =
		(uint8_t)(~PS8811_REG1_50OHM_ADJUST_CHAN_B_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_50OHM_ADJUST_CHAN_B,
			   adjust_b_50ohm_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_50OHM_ADJUST_CHAN_B, &val);
	zassert_ok(rv);
	zassert_equal(val, adjust_b_50ohm_expected);

	const uint8_t b_swing_expected =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_SWING_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_SWING, b_swing_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_CHAN_B_SWING, &val);
	zassert_ok(rv);
	zassert_equal(val, b_swing_expected);

	const uint8_t b_de_ps_lsb_expected =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_DE_PS_LSB_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB,
			   b_de_ps_lsb_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB, &val);
	zassert_ok(rv);
	zassert_equal(val, b_de_ps_lsb_expected);

	const uint8_t b_de_ps_msb_expected =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_DE_PS_MSB_RESERVED_MASK);
	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB,
			   b_de_ps_msb_expected);
	zassert_ok(rv);
	rv = ps8811_read1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB, &val);
	zassert_ok(rv);
	zassert_equal(val, b_de_ps_msb_expected);

	/* Verify that accessing a non-existent register fails. */
	rv = ps8811_write1(0xff, 0xff);
	zassert_not_equal(rv, EC_SUCCESS);
	rv = ps8811_read1(0xff, &val);
	zassert_not_equal(rv, EC_SUCCESS);
}

ZTEST(ps8811, test_p1_update)
{
	int rv;
	uint8_t val;

	/* Verify that I2C register updates work. */
	rv = ps8811_set_reg1(PS8811_REG1_USB_AEQ_LEVEL, 0xff);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_AEQ_LEVEL, 0x0f, 0x00);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_AEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xf0);

	rv = ps8811_set_reg1(PS8811_REG1_USB_ADE_CONFIG, 0xff);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_ADE_CONFIG, 0x0f, 0x00);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_ADE_CONFIG, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xf0);

	rv = ps8811_set_reg1(PS8811_REG1_USB_BEQ_LEVEL, 0xff);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_BEQ_LEVEL, 0x0f, 0x00);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_BEQ_LEVEL, &val);
	zassert_ok(rv);
	zassert_equal(val, 0xf0);

	const uint8_t bde_mask =
		(uint8_t)(~PS8811_REG1_USB_BDE_CONFIG_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_USB_BDE_CONFIG, bde_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_BDE_CONFIG, bde_mask, 0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_BDE_CONFIG, &val);
	zassert_equal(val, 0x00);

	const uint8_t a_swing_mask =
		(uint8_t)(~PS8811_REG1_USB_CHAN_A_SWING_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_USB_CHAN_A_SWING, a_swing_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_CHAN_A_SWING, a_swing_mask, 0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_A_SWING, &val);
	zassert_equal(val, 0x00);

	const uint8_t adjust_b_50ohm_mask =
		(uint8_t)(~PS8811_REG1_50OHM_ADJUST_CHAN_B_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_50OHM_ADJUST_CHAN_B,
			     adjust_b_50ohm_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_50OHM_ADJUST_CHAN_B,
			    adjust_b_50ohm_mask, 0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_50OHM_ADJUST_CHAN_B, &val);
	zassert_equal(val, 0x00);

	const uint8_t b_swing_mask =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_SWING_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_USB_CHAN_B_SWING, b_swing_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_CHAN_B_SWING, b_swing_mask, 0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_SWING, &val);
	zassert_equal(val, 0x00);

	const uint8_t b_de_ps_lsb_mask =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_DE_PS_LSB_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB,
			     b_de_ps_lsb_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB, b_de_ps_lsb_mask,
			    0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB, &val);
	zassert_equal(val, 0x00);

	const uint8_t b_de_ps_msb_mask =
		(uint8_t)(~PS8811_REG1_USB_CHAN_B_DE_PS_MSB_RESERVED_MASK);
	rv = ps8811_set_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB,
			     b_de_ps_msb_mask);
	zassert_ok(rv);
	rv = ps8811_update1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB, b_de_ps_msb_mask,
			    0x0);
	zassert_ok(rv);
	rv = ps8811_get_reg1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB, &val);
	zassert_equal(val, 0x00);

	/* Verify that updating a non-existent register fails. */
	rv = ps8811_update1(0xff, 0xff, 0xff);
	zassert_not_equal(rv, EC_SUCCESS);
}

ZTEST(ps8811, test_reserved)
{
	int rv;

	/* Verify that writing to reserved bits results in an error. */
	rv = ps8811_write1(PS8811_REG1_USB_BDE_CONFIG,
			   PS8811_REG1_USB_BDE_CONFIG_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write1(PS8811_REG1_USB_CHAN_A_SWING,
			   PS8811_REG1_USB_CHAN_A_SWING_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write1(PS8811_REG1_50OHM_ADJUST_CHAN_B,
			   PS8811_REG1_50OHM_ADJUST_CHAN_B_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_SWING,
			   PS8811_REG1_USB_CHAN_B_SWING_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_DE_PS_LSB,
			   PS8811_REG1_USB_CHAN_B_DE_PS_LSB_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);

	rv = ps8811_write1(PS8811_REG1_USB_CHAN_B_DE_PS_MSB,
			   PS8811_REG1_USB_CHAN_B_DE_PS_MSB_RESERVED_MASK);
	zassert_not_equal(rv, EC_SUCCESS);
}
