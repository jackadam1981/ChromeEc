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
#include "emul/emul_tcpc.h"
#include "hooks.h"
#include "i2c.h"
#include "stubs.h"

#include "tcpm/tcpci.h"

#define EMUL_LABEL DT_NODELABEL(tcpc_emul)

#define TCPC_ORD DT_DEP_ORD(EMUL_LABEL)

/** Test TCPCI init and vbus level */
static void test_tcpci_init(void)
{
	struct i2c_emul *emul;
	uint16_t exp_mask;

	exp_mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED |
		   TCPC_REG_ALERT_TX_DISCARDED | TCPC_REG_ALERT_RX_STATUS |
		   TCPC_REG_ALERT_RX_HARD_RST | TCPC_REG_ALERT_CC_STATUS |
		   TCPC_REG_ALERT_FAULT | TCPC_REG_ALERT_POWER_STATUS;

	emul = tcpc_emul_get(TCPC_ORD);

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0 &
					  TCPC_FLAGS_TCPCI_REV2_0_NO_VSAFE0V;

	/* Test fail on power status read */
	i2c_common_emul_set_read_fail_reg(emul, TCPC_REG_POWER_STATUS);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	i2c_common_emul_set_read_fail_reg(emul, I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test fail on uninitialised bit set */ 
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			  TCPC_REG_POWER_STATUS_UNINIT);
	zassert_equal(EC_ERROR_TIMEOUT, tcpci_tcpm_init(USBC_PORT_C1), NULL);

	/* Test init with VBUS safe0v without vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		     NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		      NULL);
	zassert_equal(TCPC_REG_POWER_STATUS_VBUS_PRES,
		      tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(exp_mask, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK),
		      NULL);

	/* Test init with VBUS present without vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			  TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	zassert_equal(TCPC_REG_POWER_STATUS_VBUS_PRES,
		      tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(exp_mask, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK),
		      NULL);

	/* Test init with VBUS present with vsafe0f tcpc config flag */
	exp_mask |= TCPC_REG_ALERT_EXT_STATUS;
	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	zassert_equal(TCPC_REG_POWER_STATUS_VBUS_PRES,
		      tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(exp_mask, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK),
		      NULL);

	/* Test init with VBUS safe0v with vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	tcpc_emul_set_reg(emul, TCPC_REG_EXT_STATUS,
			  TCPC_REG_EXT_STATUS_SAFE0V);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	zassert_equal(TCPC_REG_POWER_STATUS_VBUS_PRES,
		      tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(exp_mask, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK),
		      NULL);

	/* Test init with VBUS not safe0v with vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_EXT_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	zassert_equal(TCPC_REG_POWER_STATUS_VBUS_PRES,
		      tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(exp_mask, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK),
		      NULL);
}

/** Test TCPCI release */
static void test_tcpci_release(void)
{
	struct i2c_emul *emul;

	emul = tcpc_emul_get(TCPC_ORD);
	tcpc_emul_set_reg16(emul, TCPC_REG_ALERT, 0xffff);

	zassert_equal(EC_SUCCESS, tcpci_tcpm_release(USBC_PORT_C1), NULL);
	zassert_equal(0, tcpc_emul_get_reg(emul, TCPC_REG_POWER_STATUS_MASK),
		      NULL);
	zassert_equal(0, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT_MASK), NULL);
	zassert_equal(0, tcpc_emul_get_reg16(emul, TCPC_REG_ALERT), NULL);
}

/** Test TCPCI get cc */
static void test_tcpci_get_cc(void)
{
	enum tcpc_cc_voltage_status cc1, cc2;
	struct i2c_emul *emul;

	emul = tcpc_emul_get(TCPC_ORD);

	/* Set DRP */
	tcpc_emul_set_reg16(emul, TCPC_REG_ROLE_CTRL,
			    TCPC_REG_ROLE_CTRL_DRP_MASK);

	/* Test DRP with open state */
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test DRP with cc1 open state, cc2 src RA */
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test DRP with cc1 src RA, cc2 src RD */
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x09);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc2, NULL);

	/* Test DRP with cc1 snk open, cc2 snk default */
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x14);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test DRP with cc1 snk 1.5, cc2 snk 3.0 */
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x1e);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc2, NULL);

	/* Test no DRP with cc1 src open, cc2 src RA */
	tcpc_emul_set_reg16(emul, TCPC_REG_ROLE_CTRL, 0x05);
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test no DRP with cc1 src RD, cc2 snk default */
	tcpc_emul_set_reg16(emul, TCPC_REG_ROLE_CTRL, 0x09);
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x06);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test no DRP with cc1 snk default, cc2 snk open */
	tcpc_emul_set_reg16(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x01);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test no DRP with cc1 snk 3.0, cc2 snk 1.5 */
	tcpc_emul_set_reg16(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpc_emul_set_reg16(emul, TCPC_REG_CC_STATUS, 0x0b);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc2, NULL);
}

/** Test TCPCI set cc */
static void test_tcpci_set_cc(void)
{
	struct i2c_emul *emul;

	emul = tcpc_emul_get(TCPC_ORD);

	/* Test setting default RP and cc open */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_USB),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_OPEN), NULL);
	zassert_equal(0x0f, tcpc_emul_get_reg(emul, TCPC_REG_ROLE_CTRL), NULL);

	/* Test error on failed role ctrl set */
	i2c_common_emul_set_write_fail_reg(emul, TCPC_REG_ROLE_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_OPEN), NULL);
	i2c_common_emul_set_write_fail_reg(emul, I2C_COMMON_EMUL_NO_FAIL_REG);


	/* Test setting 1.5 RP and cc RD */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_1A5),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RD), NULL);
	zassert_equal(0x1a, tcpc_emul_get_reg(emul, TCPC_REG_ROLE_CTRL), NULL);

	/* Test setting 3.0 RP and cc RP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_3A0),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RP), NULL);
	zassert_equal(0x25, tcpc_emul_get_reg(emul, TCPC_REG_ROLE_CTRL), NULL);

	/* Test setting 3.0 RP and cc RA */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RA), NULL);
	zassert_equal(0x20, tcpc_emul_get_reg(emul, TCPC_REG_ROLE_CTRL), NULL);
}

/** Test TCPCI set polarity */
static void test_tcpci_set_polarity(void)
{
	struct i2c_emul *emul;
	uint8_t exp_ctrl;

	emul = tcpc_emul_get(TCPC_ORD);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x1c;
	tcpc_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_ctrl);

	/* Test error on failed polarity set */
	i2c_common_emul_set_write_fail_reg(emul, TCPC_REG_TCPC_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_polarity(USBC_PORT_C1, POLARITY_CC2),
		      NULL);
	i2c_common_emul_set_write_fail_reg(emul, I2C_COMMON_EMUL_NO_FAIL_REG);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_TCPC_CTRL),
		      NULL);

	/* Test setting polarity CC2 */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC2), NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_TCPC_CTRL),
		      NULL);

	/* Test setting polarity CC1 */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC1), NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_TCPC_CTRL),
		      NULL);

	/* Test setting polarity CC2 DTS */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC2_DTS),
		      NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_TCPC_CTRL),
		      NULL);

	/* Test setting polarity CC1 DTS */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC1_DTS),
		      NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_TCPC_CTRL),
		      NULL);
}

/** Test TCPCI set vconn */
static void test_tcpci_set_vconn(void)
{
	struct i2c_emul *emul;
	uint8_t exp_ctrl;

	emul = tcpc_emul_get(TCPC_ORD);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x42;
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_CTRL, exp_ctrl);

	/* Test error on failed vconn set */
	i2c_common_emul_set_write_fail_reg(emul, TCPC_REG_POWER_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_set_vconn(USBC_PORT_C1, 1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(emul, I2C_COMMON_EMUL_NO_FAIL_REG);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_POWER_CTRL),
		      NULL);

	/* Test vconn enable */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C1, 1), NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_POWER_CTRL),
		      NULL);

	/* Test vconn disable */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C1, 0), NULL);
	zassert_equal(exp_ctrl, tcpc_emul_get_reg(emul, TCPC_REG_POWER_CTRL),
		      NULL);
}

void test_suite_tcpci(void)
{
	struct tcpc_config_t tcpc_config_temp;

	/* Copy orginal tcpc configuration */
	memcpy(&tcpc_config_temp, &tcpc_config[USBC_PORT_C1],
	       sizeof(struct tcpc_config_t));

	tcpc_config[USBC_PORT_C1].bus_type = EC_BUS_TYPE_I2C;
	tcpc_config[USBC_PORT_C1].i2c_info.port = I2C_PORT_USB_C1;
	tcpc_config[USBC_PORT_C1].i2c_info.addr_flags = DT_REG_ADDR(EMUL_LABEL);
	tcpc_config[USBC_PORT_C1].drv = &tcpci_tcpm_drv;

	//int tcpci_tcpm_set_msg_header(int port, int power_role, int data_role) simple

	//.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
	//.set_rx_enable		= &tcpci_tcpm_set_rx_enable, with above ?
	ztest_test_suite(tcpci,
			 ztest_user_unit_test(test_tcpci_init),
			 ztest_user_unit_test(test_tcpci_release),
			 ztest_user_unit_test(test_tcpci_get_cc),
			 ztest_user_unit_test(test_tcpci_set_cc),
			 ztest_user_unit_test(test_tcpci_set_polarity),
			 ztest_user_unit_test(test_tcpci_set_vconn));
	ztest_run_test_suite(tcpci);

	/* Restore orginal tcpc configuration */
	memcpy(&tcpc_config[USBC_PORT_C1], &tcpc_config_temp,
	       sizeof(struct tcpc_config_t));
}
