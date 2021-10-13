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
#include "hooks.h"
#include "i2c.h"
#include "stubs.h"

#include "tcpm/tcpci.h"

#define EMUL_LABEL DT_NODELABEL(tcpci_emul)

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

/** Test TCPCI init and vbus level */
static void test_tcpci_init(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint16_t exp_mask;

	i2c_emul = tcpci_emul_get_i2c_emul(emul);

	exp_mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED |
		   TCPC_REG_ALERT_TX_DISCARDED | TCPC_REG_ALERT_RX_STATUS |
		   TCPC_REG_ALERT_RX_HARD_RST | TCPC_REG_ALERT_CC_STATUS |
		   TCPC_REG_ALERT_FAULT | TCPC_REG_ALERT_POWER_STATUS;

	tcpc_config[USBC_PORT_C0].flags = TCPC_FLAGS_TCPCI_REV2_0 &
					  TCPC_FLAGS_TCPCI_REV2_0_NO_VSAFE0V;
	tcpci_emul_set_rev(emul, TCPCI_EMUL_REV2_0_VER1_1);

	/* Test fail on power status read */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_POWER_STATUS);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test fail on uninitialised bit set */
	tcpci_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_UNINIT);
	zassert_equal(EC_ERROR_TIMEOUT, tcpci_tcpm_init(USBC_PORT_C0), NULL);

	/* Test init with VBUS safe0v without vsafe0f tcpc config flag */
	tcpci_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_SAFE0V),
		     NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_PRESENT),
		      NULL);
	check_tcpci_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
			TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS present without vsafe0f tcpc config flag */
	tcpci_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			   TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_PRESENT),
		     NULL);
	check_tcpci_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
			TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS present with vsafe0f tcpc config flag */
	exp_mask |= TCPC_REG_ALERT_EXT_STATUS;
	tcpc_config[USBC_PORT_C0].flags = TCPC_FLAGS_TCPCI_REV2_0;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_PRESENT),
		     NULL);
	check_tcpci_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
			TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS safe0v with vsafe0f tcpc config flag */
	tcpci_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	tcpci_emul_set_reg(emul, TCPC_REG_EXT_STATUS,
			   TCPC_REG_EXT_STATUS_SAFE0V);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_PRESENT),
		     NULL);
	check_tcpci_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
			TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS not safe0v with vsafe0f tcpc config flag */
	tcpci_emul_set_reg(emul, TCPC_REG_EXT_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C0), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C0, VBUS_PRESENT),
		     NULL);
	check_tcpci_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
			TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);
}

/** Test TCPCI release */
static void test_tcpci_release(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpci_emul_set_reg(emul, TCPC_REG_ALERT, 0xffff);

	zassert_equal(EC_SUCCESS, tcpci_tcpm_release(USBC_PORT_C0), NULL);
	check_tcpci_reg(emul, 0, TCPC_REG_POWER_STATUS_MASK);
	check_tcpci_reg(emul, 0, TCPC_REG_ALERT_MASK);
	check_tcpci_reg(emul, 0, TCPC_REG_ALERT);
}

/** Test TCPCI get cc */
static void test_tcpci_get_cc(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	enum tcpc_cc_voltage_status cc1, cc2;

	/* Set DRP */
	tcpci_emul_set_reg(emul, TCPC_REG_ROLE_CTRL,
			   TCPC_REG_ROLE_CTRL_DRP_MASK);

	/* Test DRP with open state */
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test DRP with cc1 open state, cc2 src RA */
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test DRP with cc1 src RA, cc2 src RD */
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x09);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc2, NULL);

	/* Test DRP with cc1 snk open, cc2 snk default */
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x14);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test DRP with cc1 snk 1.5, cc2 snk 3.0 */
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x1e);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc2, NULL);

	/* Test no DRP with cc1 src open, cc2 src RA */
	tcpci_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x05);
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test no DRP with cc1 src RD, cc2 snk default */
	tcpci_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x09);
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x06);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test no DRP with cc1 snk default, cc2 snk open */
	tcpci_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x01);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test no DRP with cc1 snk 3.0, cc2 snk 1.5 */
	tcpci_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpci_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x0b);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C0, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc2, NULL);
}

/** Test TCPCI set cc */
static void test_tcpci_set_cc(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpci_emul_get_i2c_emul(emul);

	/* Test setting default RP and cc open */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C0, TYPEC_RP_USB),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C0, TYPEC_CC_OPEN), NULL);
	check_tcpci_reg(emul, 0x0f, TCPC_REG_ROLE_CTRL);

	/* Test error on failed role ctrl set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ROLE_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_cc(USBC_PORT_C0, TYPEC_CC_OPEN), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);


	/* Test setting 1.5 RP and cc RD */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C0, TYPEC_RP_1A5),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C0, TYPEC_CC_RD), NULL);
	check_tcpci_reg(emul, 0x1a, TCPC_REG_ROLE_CTRL);

	/* Test setting 3.0 RP and cc RP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C0, TYPEC_RP_3A0),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C0, TYPEC_CC_RP), NULL);
	check_tcpci_reg(emul, 0x25, TCPC_REG_ROLE_CTRL);

	/* Test setting 3.0 RP and cc RA */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C0, TYPEC_CC_RA), NULL);
	check_tcpci_reg(emul, 0x20, TCPC_REG_ROLE_CTRL);
}

/** Test TCPCI set polarity */
static void test_tcpci_set_polarity(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint8_t exp_ctrl;

	i2c_emul = tcpci_emul_get_i2c_emul(emul);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x1c;
	tcpci_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_ctrl);

	/* Test error on failed polarity set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TCPC_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_polarity(USBC_PORT_C0, POLARITY_CC2),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC2 */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C0,
							  POLARITY_CC2), NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC1 */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C0,
							  POLARITY_CC1), NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC2 DTS */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C0,
							  POLARITY_CC2_DTS),
		      NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC1 DTS */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C0,
							  POLARITY_CC1_DTS),
		      NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);
}

/** Test TCPCI set vconn */
static void test_tcpci_set_vconn(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint8_t exp_ctrl;

	i2c_emul = tcpci_emul_get_i2c_emul(emul);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x42;
	tcpci_emul_set_reg(emul, TCPC_REG_POWER_CTRL, exp_ctrl);

	/* Test error on failed vconn set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_POWER_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_set_vconn(USBC_PORT_C0, 1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);

	/* Test vconn enable */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C0, 1), NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);

	/* Test vconn disable */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C0, 0), NULL);
	check_tcpci_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);
}

void test_suite_tcpci(void)
{
	ztest_test_suite(tcpci,
			 ztest_user_unit_test(test_tcpci_init),
			 ztest_user_unit_test(test_tcpci_release),
			 ztest_user_unit_test(test_tcpci_get_cc),
			 ztest_user_unit_test(test_tcpci_set_cc),
			 ztest_user_unit_test(test_tcpci_set_polarity),
			 ztest_user_unit_test(test_tcpci_set_vconn));
	ztest_run_test_suite(tcpci);
}
