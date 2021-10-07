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

/** Check TCPC register value */
static void check_tcpc_reg_f(const struct emul *emul, uint16_t exp_val, int reg,
			     int line)
{
	uint16_t reg_val;

	zassert_ok(tcpc_emul_get_reg(emul, reg, &reg_val),
		   "Failed tcpc_emul_get_reg(); line: %d", line);
	zassert_equal(exp_val, reg_val, "Expected 0x%x, got 0x%x; line: %d",
		      exp_val, reg_val, line);
}
#define check_tcpc_reg(emul, exp_val, reg)		\
	check_tcpc_reg_f(emul, exp_val, reg, __LINE__)

/** Test TCPCI init and vbus level */
static void test_tcpci_init(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint16_t exp_mask;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	exp_mask = TCPC_REG_ALERT_TX_SUCCESS | TCPC_REG_ALERT_TX_FAILED |
		   TCPC_REG_ALERT_TX_DISCARDED | TCPC_REG_ALERT_RX_STATUS |
		   TCPC_REG_ALERT_RX_HARD_RST | TCPC_REG_ALERT_CC_STATUS |
		   TCPC_REG_ALERT_FAULT | TCPC_REG_ALERT_POWER_STATUS;

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0 &
					  TCPC_FLAGS_TCPCI_REV2_0_NO_VSAFE0V;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	/* Test fail on power status read */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_POWER_STATUS);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

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
	check_tcpc_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
		       TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS present without vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			  TCPC_REG_POWER_STATUS_VBUS_PRES);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	check_tcpc_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
		       TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS present with vsafe0f tcpc config flag */
	exp_mask |= TCPC_REG_ALERT_EXT_STATUS;
	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	check_tcpc_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
		       TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS safe0v with vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	tcpc_emul_set_reg(emul, TCPC_REG_EXT_STATUS,
			  TCPC_REG_EXT_STATUS_SAFE0V);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_true(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	check_tcpc_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
		       TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/* Test init with VBUS not safe0v with vsafe0f tcpc config flag */
	tcpc_emul_set_reg(emul, TCPC_REG_EXT_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_init(USBC_PORT_C1), NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_SAFE0V),
		      NULL);
	zassert_false(tcpci_tcpm_check_vbus_level(USBC_PORT_C1, VBUS_PRESENT),
		     NULL);
	check_tcpc_reg(emul, TCPC_REG_POWER_STATUS_VBUS_PRES,
		       TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);
}

/** Test TCPCI release */
static void test_tcpci_release(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, 0xffff);

	zassert_equal(EC_SUCCESS, tcpci_tcpm_release(USBC_PORT_C1), NULL);
	check_tcpc_reg(emul, 0, TCPC_REG_POWER_STATUS_MASK);
	check_tcpc_reg(emul, 0, TCPC_REG_ALERT_MASK);
	check_tcpc_reg(emul, 0, TCPC_REG_ALERT);
}

/** Test TCPCI get cc */
static void test_tcpci_get_cc(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	enum tcpc_cc_voltage_status cc1, cc2;

	/* Set DRP */
	tcpc_emul_set_reg(emul, TCPC_REG_ROLE_CTRL,
			    TCPC_REG_ROLE_CTRL_DRP_MASK);

	/* Test DRP with open state */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test DRP with cc1 open state, cc2 src RA */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test DRP with cc1 src RA, cc2 src RD */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x09);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc2, NULL);

	/* Test DRP with cc1 snk open, cc2 snk default */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x14);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test DRP with cc1 snk 1.5, cc2 snk 3.0 */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x1e);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc2, NULL);

	/* Test no DRP with cc1 src open, cc2 src RA */
	tcpc_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x05);
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x04);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RA, cc2, NULL);

	/* Test no DRP with cc1 src RD, cc2 snk default */
	tcpc_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x09);
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x06);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RD, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc2, NULL);

	/* Test no DRP with cc1 snk default, cc2 snk open */
	tcpc_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x01);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_DEF, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_OPEN, cc2, NULL);

	/* Test no DRP with cc1 snk 3.0, cc2 snk 1.5 */
	tcpc_emul_set_reg(emul, TCPC_REG_ROLE_CTRL, 0x0a);
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x0b);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_get_cc(USBC_PORT_C1, &cc1, &cc2),
		      NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_3_0, cc1, NULL);
	zassert_equal(TYPEC_CC_VOLT_RP_1_5, cc2, NULL);
}

/** Test TCPCI set cc */
static void test_tcpci_set_cc(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test setting default RP and cc open */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_USB),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_OPEN), NULL);
	check_tcpc_reg(emul, 0x0f, TCPC_REG_ROLE_CTRL);

	/* Test error on failed role ctrl set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ROLE_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_OPEN), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);


	/* Test setting 1.5 RP and cc RD */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_1A5),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RD), NULL);
	check_tcpc_reg(emul, 0x1a, TCPC_REG_ROLE_CTRL);

	/* Test setting 3.0 RP and cc RP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_select_rp_value(USBC_PORT_C1, TYPEC_RP_3A0),
		      NULL);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RP), NULL);
	check_tcpc_reg(emul, 0x25, TCPC_REG_ROLE_CTRL);

	/* Test setting 3.0 RP and cc RA */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_cc(USBC_PORT_C1, TYPEC_CC_RA), NULL);
	check_tcpc_reg(emul, 0x20, TCPC_REG_ROLE_CTRL);
}

/** Test TCPCI set polarity */
static void test_tcpci_set_polarity(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint8_t exp_ctrl;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x1c;
	tcpc_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_ctrl);

	/* Test error on failed polarity set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TCPC_CTRL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_polarity(USBC_PORT_C1, POLARITY_CC2),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC2 */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC2), NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC1 */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC1), NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC2 DTS */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC2_DTS),
		      NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);

	/* Test setting polarity CC1 DTS */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_polarity(USBC_PORT_C1,
							  POLARITY_CC1_DTS),
		      NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);
}

/** Test TCPCI set vconn */
static void test_tcpci_set_vconn(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	uint8_t exp_ctrl;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Only bit 0 should be changed */
	exp_ctrl = 0x42;
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_CTRL, exp_ctrl);

	/* Test error on failed vconn set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_POWER_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_set_vconn(USBC_PORT_C1, 1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);

	/* Test vconn enable */
	exp_ctrl |= 0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C1, 1), NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);

	/* Test vconn disable */
	exp_ctrl &= ~0x1;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_vconn(USBC_PORT_C1, 0), NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);
}

/** Test TCPCI set msg header */
static void test_tcpci_set_msg_header(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test error on failed header set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_MSG_HDR_INFO);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_set_msg_header(USBC_PORT_C1, PD_ROLE_SINK,
						PD_ROLE_UFP), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test setting sink UFP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_msg_header(USBC_PORT_C1, PD_ROLE_SINK,
						PD_ROLE_UFP), NULL);
	check_tcpc_reg(emul, 0x02, TCPC_REG_MSG_HDR_INFO);

	/* Test setting sink DFP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_msg_header(USBC_PORT_C1, PD_ROLE_SINK,
						PD_ROLE_DFP), NULL);
	check_tcpc_reg(emul, 0x0a, TCPC_REG_MSG_HDR_INFO);

	/* Test setting source UFP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_msg_header(USBC_PORT_C1, PD_ROLE_SOURCE,
						PD_ROLE_UFP), NULL);
	check_tcpc_reg(emul, 0x03, TCPC_REG_MSG_HDR_INFO);

	/* Test setting source DFP */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_set_msg_header(USBC_PORT_C1, PD_ROLE_SOURCE,
						PD_ROLE_DFP), NULL);
	check_tcpc_reg(emul, 0x0b, TCPC_REG_MSG_HDR_INFO);
}

/** Test TCPCI rx and sop prime enable */
static void test_tcpci_set_rx_detect(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test error from rx_enable on rx detect set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_RX_DETECT);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_set_rx_enable(USBC_PORT_C1, 1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test rx disable */
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_rx_enable(USBC_PORT_C1, 0),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_RX_DETECT);

	/* Test setting sop prime with rx disable doesn't change RX_DETECT */
	zassert_equal(EC_SUCCESS, tcpci_tcpm_sop_prime_enable(USBC_PORT_C1, 1),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_RX_DETECT);

	/* Test that enabling rx after sop prime will set RX_DETECT properly */
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_rx_enable(USBC_PORT_C1, 1),
		      NULL);
	check_tcpc_reg(emul, 0x27, TCPC_REG_RX_DETECT);

	/* Test error from sop_prime on rx detect set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_RX_DETECT);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_sop_prime_enable(USBC_PORT_C1, 0), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test disabling sop prime with rx enabled does change RX_DETECT */
	zassert_equal(EC_SUCCESS, tcpci_tcpm_sop_prime_enable(USBC_PORT_C1, 0),
		      NULL);
	check_tcpc_reg(emul, 0x21, TCPC_REG_RX_DETECT);

	/* Test that enabling rx after disabling sop prime set RX_DETECT */
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_rx_enable(USBC_PORT_C1, 0),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_RX_DETECT);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_set_rx_enable(USBC_PORT_C1, 1),
		      NULL);
	check_tcpc_reg(emul, 0x21, TCPC_REG_RX_DETECT);
}

/** Test TCPCI get raw message from TCPC */
static void test_tcpci_get_rx_message_raw(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	struct tcpc_emul_msg msg;
	uint32_t payload[7];
	uint8_t buf[32];
	int exp_head;
	int i, head;
	int size;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, 0x0);
	tcpc_emul_set_reg(emul, TCPC_REG_DEV_CAP_2,
			    TCPC_REG_DEV_CAP_2_LONG_MSG);

	for (i = 0; i < 32; i++) {
		buf[i] = i + 1;
	}
	msg.buf = buf;
	msg.cnt = 32;
	msg.type = TCPCI_MSG_SOP;
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg, true),
		   "Failed to setup emulator message");

	/* Test fail on reading byte count */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_RX_BUFFER);
	zassert_equal(EC_ERROR_UNKNOWN,
		      tcpci_tcpm_get_message_raw(USBC_PORT_C1, payload, &head),
		      NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	/* Get raw message should always clean RX alerts */
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Test too short message */
	msg.cnt = 2;
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg, true),
		   "Failed to setup emulator message");
	zassert_equal(EC_ERROR_UNKNOWN,
		      tcpci_tcpm_get_message_raw(USBC_PORT_C1, payload, &head),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Test too long message */
	msg.cnt = 32;
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg, true),
		   "Failed to setup emulator message");
	zassert_equal(EC_ERROR_UNKNOWN,
		      tcpci_tcpm_get_message_raw(USBC_PORT_C1, payload, &head),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Test alert register and message payload on success */
	size = 28;
	msg.cnt = size + 3;
	msg.type = TCPCI_MSG_SOP_PRIME;
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg, true),
		   "Failed to setup emulator message");
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_get_message_raw(USBC_PORT_C1, payload, &head),
		      NULL);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);
	/*
	 * Type is in bits 31-28 of header, buf[0] is in bits 7-0,
	 * buf[1] is in bits 15-8
	 */
	exp_head = (TCPCI_MSG_SOP_PRIME << 28) | (buf[1] << 8) | buf[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf + 2, size, NULL);
}

/** Test TCPCI get raw message from TCPC revision 2.0 */
static void test_tcpci_get_rx_message_raw_rev2(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	test_tcpci_get_rx_message_raw();
}

/** Test TCPCI get raw message from TCPC revision 1.0 */
static void test_tcpci_get_rx_message_raw_rev1(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpc_config[USBC_PORT_C1].flags = 0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV1_0_VER1_0);

	test_tcpci_get_rx_message_raw();
}

/** Test TCPCI transmitting message from TCPC */
static void test_tcpci_transmit(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct tcpc_emul_msg *msg;
	struct i2c_emul *i2c_emul;
	uint32_t data[6];
	uint16_t header;
	int i;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	msg = tcpc_emul_get_tx_msg(emul);

	/* Fill transmit data with pattern */
	for (i = 0; i < 6 * sizeof(uint32_t); i++) {
		((uint8_t *)data)[i] = i;
	}

	/* Test transmit hard reset fail */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TRANSMIT);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_TX_HARD_RESET,
					  0, NULL), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test transmit cabel reset */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_CABLE_RESET,
					  0, NULL), NULL);
	zassert_equal(TCPCI_MSG_CABLE_RESET, msg->type, NULL);

	/* Test transmit hard reset */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_TX_HARD_RESET,
					  0, NULL), NULL);
	zassert_equal(TCPCI_MSG_TX_HARD_RESET, msg->type, NULL);

	/* Test transmit fail on rx buffer */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TX_BUFFER);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_SOP_PRIME,
					  0, data), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test transmit only header */
	/* Build random header with count 0 */
	header = PD_HEADER(PD_CTRL_PING, PD_ROLE_SOURCE, PD_ROLE_UFP, 0, 0,
			   PD_REV20, 0);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_SOP_PRIME,
					  header, data), NULL);
	zassert_equal(TCPCI_MSG_SOP_PRIME, msg->type, NULL);
	zassert_mem_equal(msg->buf, &header, 2, NULL);
	zassert_equal(2, msg->cnt, NULL);

	/* Test transmit message */
	/* Build random header with count 6 */
	header = PD_HEADER(PD_CTRL_PING, PD_ROLE_SOURCE, PD_ROLE_UFP, 0, 6,
			   PD_REV20, 0);
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_transmit(USBC_PORT_C1, TCPCI_MSG_SOP_PRIME,
					  header, data), NULL);
	zassert_equal(TCPCI_MSG_SOP_PRIME, msg->type, NULL);
	zassert_mem_equal(msg->buf, &header, 2, NULL);
	zassert_mem_equal(msg->buf + 2, data, 6 * sizeof(uint32_t), NULL);
	zassert_equal(2 + 6 * sizeof(uint32_t), msg->cnt, NULL);
}

/** Test TCPCI transmitting message from TCPC revision 2.0 */
static void test_tcpci_transmit_rev2(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	test_tcpci_transmit();
}

/** Test TCPCI transmitting message from TCPC revision 1.0 */
static void test_tcpci_transmit_rev1(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));

	tcpc_config[USBC_PORT_C1].flags = 0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV1_0_VER1_0);

	test_tcpci_transmit();
}

/** Test TCPCI alert */
static void test_tcpci_alert(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	/* Test alert read fail */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_ALERT);
	tcpci_tcpc_alert(USBC_PORT_C1);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Handle overcurrent */
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, TCPC_REG_ALERT_FAULT);
	tcpc_emul_set_reg(emul, TCPC_REG_FAULT_STATUS,
			    TCPC_REG_FAULT_STATUS_VCONN_OVER_CURRENT);
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);
	check_tcpc_reg(emul, 0x0, TCPC_REG_FAULT_STATUS);

	/* Test TX complete */
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, TCPC_REG_ALERT_TX_COMPLETE);
	tcpci_tcpc_alert(USBC_PORT_C1);

	/* Test clear alert and ext_alert */
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, TCPC_REG_ALERT_ALERT_EXT);
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT_EXT,
			  TCPC_REG_ALERT_EXT_TIMER_EXPIRED);
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);
	check_tcpc_reg(emul, 0x0, TCPC_REG_FAULT_STATUS);

	/* Test CC changed */
	tcpc_emul_set_reg(emul, TCPC_REG_CC_STATUS, 0x1e);
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, TCPC_REG_ALERT_CC_STATUS);
	tcpci_tcpc_alert(USBC_PORT_C1);

	/* Test Hard reset */
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, TCPC_REG_ALERT_RX_HARD_RST);
	tcpci_tcpc_alert(USBC_PORT_C1);
}


/** Test TCPCI alert RX message */
static void test_tcpci_alert_rx_message(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct tcpc_emul_msg msg1, msg2;
	uint8_t buf1[32], buf2[32];
	uint32_t payload[7];
	int exp_head;
	int i, head;
	int size;

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	for (i = 0; i < 32; i++) {
		buf1[i] = i + 1;
		buf2[i] = i + 33;
	}
	size = 23;
	msg1.buf = buf1;
	msg1.cnt = size + 3;
	msg1.type = TCPCI_MSG_SOP;

	msg2.buf = buf2;
	msg2.cnt = size + 3;
	msg2.type = TCPCI_MSG_SOP_PRIME;

	/* Test receiving one message */
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg1, true),
		   "Failed to setup emulator message");
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Check if msg1 is in queue */
	zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
	zassert_equal(EC_SUCCESS, tcpm_dequeue_message(USBC_PORT_C1, payload,
						       &head), NULL);
	exp_head = (TCPCI_MSG_SOP << 28) | (buf1[1] << 8) | buf1[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf1 + 2, size, NULL);
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);

	/* Test receiving two messages */
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg1, true),
		   "Failed to setup emulator message");
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg2, true),
		   "Failed to setup emulator message");
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Check if msg1 is in queue */
	zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
	zassert_equal(EC_SUCCESS, tcpm_dequeue_message(USBC_PORT_C1, payload,
						       &head), NULL);
	exp_head = (TCPCI_MSG_SOP << 28) | (buf1[1] << 8) | buf1[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf1 + 2, size, NULL);
	/* Check if msg2 is in queue */
	zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
	zassert_equal(EC_SUCCESS, tcpm_dequeue_message(USBC_PORT_C1, payload,
						       &head), NULL);
	exp_head = (TCPCI_MSG_SOP_PRIME << 28) | (buf2[1] << 8) | buf2[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf2 + 2, size, NULL);
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);

	/* Test with too long first message */
	msg1.cnt = 32;
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg1, true),
		   "Failed to setup emulator message");
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg2, true),
		   "Failed to setup emulator message");
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Check if msg2 is in queue */
	zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
	zassert_equal(EC_SUCCESS, tcpm_dequeue_message(USBC_PORT_C1, payload,
						       &head), NULL);
	exp_head = (TCPCI_MSG_SOP_PRIME << 28) | (buf2[1] << 8) | buf2[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf2 + 2, size, NULL);
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);

	/* Test constant read message failure */
	zassert_ok(tcpc_emul_add_rx_msg(emul, &msg1, true),
		   "Failed to setup emulator message");
	/* Create loop with one message with wrong size */
	msg1.next = &msg1;
	tcpci_tcpc_alert(USBC_PORT_C1);
	/* Nothing should be in queue */
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);

	/* Test constant correct messages stream */
	msg1.cnt = size + 3;
	tcpci_tcpc_alert(USBC_PORT_C1);
	msg1.next = NULL;

	/* msg1 should be at least twice in queue */
	exp_head = (TCPCI_MSG_SOP << 28) | (buf1[1] << 8) | buf1[0];
	for (i = 0; i < 2; i++) {
		zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
		zassert_equal(EC_SUCCESS,
			      tcpm_dequeue_message(USBC_PORT_C1, payload,
						   &head), NULL);
		zassert_equal(exp_head, head,
			      "Received header 0x%08lx, expected 0x%08lx",
			      head, exp_head);
		zassert_mem_equal(payload, buf1 + 2, size, NULL);
	}
	tcpm_clear_pending_messages(USBC_PORT_C1);
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);

	/* Read message that is left in TCPC buffer */
	tcpci_tcpc_alert(USBC_PORT_C1);
	check_tcpc_reg(emul, 0x0, TCPC_REG_ALERT);

	/* Check if msg1 is in queue */
	zassert_true(tcpm_has_pending_message(USBC_PORT_C1), NULL);
	zassert_equal(EC_SUCCESS, tcpm_dequeue_message(USBC_PORT_C1, payload,
						       &head), NULL);
	exp_head = (TCPCI_MSG_SOP << 28) | (buf1[1] << 8) | buf1[0];
	zassert_equal(exp_head, head,
		      "Received header 0x%08lx, expected 0x%08lx",
		      head, exp_head);
	zassert_mem_equal(payload, buf1 + 2, size, NULL);
	zassert_false(tcpm_has_pending_message(USBC_PORT_C1), NULL);
}

/** Test TCPCI auto discharge on disconnect */
static void test_tcpci_auto_discharge(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	uint8_t exp_ctrl;

	/* Only bit 4 should be changed */
	exp_ctrl = 0x42;
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_CTRL, exp_ctrl);

	/* Test discharge enable */
	exp_ctrl |= 0x10;
	tcpci_tcpc_enable_auto_discharge_disconnect(USBC_PORT_C1, 1);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);

	/* Test discharge disable */
	exp_ctrl &= ~0x10;
	tcpci_tcpc_enable_auto_discharge_disconnect(USBC_PORT_C1, 0);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_POWER_CTRL);
}

/** Test TCPCI drp toggle */
static void test_tcpci_drp_toggle(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	uint8_t exp_tcpc_ctrl, exp_role_ctrl;
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	tcpc_config[USBC_PORT_C1].flags = TCPC_FLAGS_TCPCI_REV2_0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV2_0_VER1_1);

	/* Test error on failed role CTRL set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ROLE_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpc_drp_toggle(USBC_PORT_C1),
		      NULL);

	/* Test error on failed TCPC CTRL set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TCPC_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpc_drp_toggle(USBC_PORT_C1),
		      NULL);

	/* Test error on failed command set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_COMMAND);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpc_drp_toggle(USBC_PORT_C1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test correct registers values for rev 2.0 */
	exp_role_ctrl = 0x45;
	/* Only look for connection bit (6) should be changed  */
	exp_tcpc_ctrl = 0x9;
	tcpc_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_tcpc_ctrl);
	exp_tcpc_ctrl |= 0x40;
	zassert_equal(EC_SUCCESS, tcpci_tcpc_drp_toggle(USBC_PORT_C1), NULL);
	check_tcpc_reg(emul, exp_tcpc_ctrl, TCPC_REG_TCPC_CTRL);
	check_tcpc_reg(emul, exp_role_ctrl, TCPC_REG_ROLE_CTRL);
	check_tcpc_reg(emul, TCPC_REG_COMMAND_LOOK4CONNECTION,
		       TCPC_REG_COMMAND);

	/* Test correct registers values for rev 1.0 */
	tcpc_config[USBC_PORT_C1].flags = 0;
	tcpc_emul_set_rev(emul, TCPC_EMUL_REV1_0_VER1_0);
	exp_role_ctrl = 0x4a;
	/* Only look for connection bit (6) should be changed  */
	exp_tcpc_ctrl = 0x9;
	tcpc_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_tcpc_ctrl);
	exp_tcpc_ctrl |= 0x40;
	zassert_equal(EC_SUCCESS, tcpci_tcpc_drp_toggle(USBC_PORT_C1), NULL);
	check_tcpc_reg(emul, exp_tcpc_ctrl, TCPC_REG_TCPC_CTRL);
	check_tcpc_reg(emul, exp_role_ctrl, TCPC_REG_ROLE_CTRL);
	check_tcpc_reg(emul, TCPC_REG_COMMAND_LOOK4CONNECTION,
		       TCPC_REG_COMMAND);
}

/** Test TCPCI get chip info */
static void test_tcpci_get_chip_info(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct ec_response_pd_chip_info_v1 info;
	uint16_t vendor, product, bcd;
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test error on failed vendor id get */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_VENDOR_ID);
	zassert_equal(EC_ERROR_INVAL, tcpci_get_chip_info(USBC_PORT_C1, 1,
							  &info), NULL);

	/* Test error on failed product id get */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_PRODUCT_ID);
	zassert_equal(EC_ERROR_INVAL, tcpci_get_chip_info(USBC_PORT_C1, 1,
							  &info), NULL);

	/* Test error on failed BCD get */
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_VENDOR_ID);
	zassert_equal(EC_ERROR_INVAL, tcpci_get_chip_info(USBC_PORT_C1, 1,
							  &info), NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test reading chip info */
	vendor = 0x1234;
	product = 0x5678;
	bcd = 0x9876;
	tcpc_emul_set_reg(emul, TCPC_REG_VENDOR_ID, vendor);
	tcpc_emul_set_reg(emul, TCPC_REG_PRODUCT_ID, product);
	tcpc_emul_set_reg(emul, TCPC_REG_BCD_DEV, bcd);
	zassert_equal(EC_SUCCESS, tcpci_get_chip_info(USBC_PORT_C1, 1, &info),
		      NULL);
	zassert_equal(vendor, info.vendor_id, NULL);
	zassert_equal(product, info.product_id, NULL);
	zassert_equal(bcd, info.device_id, NULL);

	/* Test reading cached chip info */
	info.vendor_id = 0;
	info.product_id = 0;
	info.device_id = 0;
	/* Make sure, that TCPC is not accessed */
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_FAIL_ALL_REG);
	zassert_equal(EC_SUCCESS, tcpci_get_chip_info(USBC_PORT_C1, 0, &info),
		      NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);
	zassert_equal(vendor, info.vendor_id, NULL);
	zassert_equal(product, info.product_id, NULL);
	zassert_equal(bcd, info.device_id, NULL);
}

/** Test TCPCI enter low power mode */
static void test_tcpci_low_power_mode(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test error on failed command set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_COMMAND);
	zassert_equal(EC_ERROR_INVAL, tcpci_enter_low_power_mode(USBC_PORT_C1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test correct command is issued */
	zassert_equal(EC_SUCCESS, tcpci_enter_low_power_mode(USBC_PORT_C1),
		      NULL);
	check_tcpc_reg(emul, TCPC_REG_COMMAND_I2CIDLE, TCPC_REG_COMMAND);
}

/** Test TCPCI set bist test mode */
static void test_tcpci_set_bist_mode(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	uint16_t exp_mask;
	uint8_t exp_ctrl;
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Test error on TCPC CTRL set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_TCPC_CTRL);
	zassert_equal(EC_ERROR_INVAL, tcpci_set_bist_test_mode(USBC_PORT_C1, 1),
		      NULL);

	/* Test error on alert mask set */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ALERT_MASK);
	zassert_equal(EC_ERROR_INVAL, tcpci_set_bist_test_mode(USBC_PORT_C1, 1),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test enabling bist test mode */
	exp_mask = 0x7fff;
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT_MASK, exp_mask);
	/* RX status alert should be disabled */
	exp_mask &= ~0x4;
	exp_ctrl = 0x45;
	tcpc_emul_set_reg(emul, TCPC_REG_TCPC_CTRL, exp_ctrl);
	/* BIST test mode should be enabled */
	exp_ctrl |= 0x2;
	zassert_equal(EC_SUCCESS, tcpci_set_bist_test_mode(USBC_PORT_C1, 1),
		      NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);

	/*
	 * Test disabling bist test mode. RX status alert should be enabled and
	 * BIST test mode should be disabled.
	 */
	exp_mask |= 0x4;
	exp_ctrl &= ~0x2;
	zassert_equal(EC_SUCCESS, tcpci_set_bist_test_mode(USBC_PORT_C1, 0),
		      NULL);
	check_tcpc_reg(emul, exp_ctrl, TCPC_REG_TCPC_CTRL);
	check_tcpc_reg(emul, exp_mask, TCPC_REG_ALERT_MASK);
}

/** Test TCPC xfer */
static void test_tcpc_xfer(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	uint16_t val, exp_val;
	uint8_t reg;

	exp_val = 0x7fff;
	reg = TCPC_REG_ALERT_MASK;
	tcpc_emul_set_reg(emul, reg, exp_val);
	zassert_equal(EC_SUCCESS,
		      tcpc_xfer(USBC_PORT_C1, &reg, 1, (uint8_t *)&val, 2),
		      NULL);
	zassert_equal(exp_val, val, "0x%x != 0x%x", exp_val, val);
}

/** Test TCPCI debug accessory enable/disable */
static void test_tcpci_debug_accessory(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	uint8_t exp_val;

	/* Only bit 6 should be changed */
	exp_val = 0x42;
	tcpc_emul_set_reg(emul, TCPC_REG_CONFIG_STD_OUTPUT, exp_val);

	/* Test discharge enable (disbale bit 6) */
	exp_val &= ~0x40;
	tcpci_tcpc_debug_accessory(USBC_PORT_C1, 1);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);

	/* Test discharge disable (enable bit 6) */
	exp_val |= 0x40;
	tcpci_tcpc_debug_accessory(USBC_PORT_C1, 0);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);
}

/** Structure used in TCPC usb mux tests */
struct usb_mux tcpc_usb_mux = {
	.usb_port = USBC_PORT_C1,
	.driver = &tcpci_tcpm_usb_mux_driver,
	.i2c_port = I2C_PORT_USB_C1,
	.i2c_addr_flags = DT_REG_ADDR(EMUL_LABEL),
};

/** Test TCPCI mux init */
static void test_tcpci_mux_init(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Make sure, that TCPC is not accessed */
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_FAIL_ALL_REG);
	tcpc_usb_mux.flags = 0;
	zassert_equal(EC_SUCCESS, tcpci_tcpm_mux_init(&tcpc_usb_mux), NULL);

	/* Test fail on power status read */
	tcpc_usb_mux.flags = USB_MUX_FLAG_NOT_TCPC;
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_POWER_STATUS);
	zassert_equal(EC_ERROR_INVAL, tcpci_tcpm_mux_init(&tcpc_usb_mux), NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test fail on uninitialised bit set */
	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS,
			  TCPC_REG_POWER_STATUS_UNINIT);
	zassert_equal(EC_ERROR_TIMEOUT, tcpci_tcpm_mux_init(&tcpc_usb_mux),
		      NULL);

	tcpc_emul_set_reg(emul, TCPC_REG_POWER_STATUS, 0);
	/* Test fail on alert mask write fail */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ALERT_MASK);
	zassert_equal(EC_ERROR_UNKNOWN, tcpci_tcpm_mux_init(&tcpc_usb_mux),
		      NULL);

	/* Test fail on alert write fail */
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_ALERT);
	zassert_equal(EC_ERROR_UNKNOWN, tcpci_tcpm_mux_init(&tcpc_usb_mux),
		      NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test success init */
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT, 0xffff);
	tcpc_emul_set_reg(emul, TCPC_REG_ALERT_MASK, 0xffff);
	zassert_equal(EC_SUCCESS, tcpci_tcpm_mux_init(&tcpc_usb_mux), NULL);
	check_tcpc_reg(emul, 0, TCPC_REG_ALERT_MASK);
	check_tcpc_reg(emul, 0, TCPC_REG_ALERT);
}

/** Test TCPCI mux enter low power mode */
static void test_tcpci_mux_enter_low_power(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);

	/* Make sure, that TCPC is not accessed */
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_FAIL_ALL_REG);
	tcpc_usb_mux.flags = 0;
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_enter_low_power(&tcpc_usb_mux), NULL);

	/* Test error on failed command set */
	tcpc_usb_mux.flags = USB_MUX_FLAG_NOT_TCPC;
	i2c_common_emul_set_write_fail_reg(i2c_emul, TCPC_REG_COMMAND);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_mux_enter_low_power(&tcpc_usb_mux), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test correct command is issued */
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_enter_low_power(&tcpc_usb_mux), NULL);
	check_tcpc_reg(emul, TCPC_REG_COMMAND_I2CIDLE, TCPC_REG_COMMAND);
}

/** Test TCPCI mux set and get */
static void test_tcpci_mux_set_get(void)
{
	const struct emul *emul = emul_get_binding(DT_LABEL(EMUL_LABEL));
	struct i2c_emul *i2c_emul;
	mux_state_t mux_state, mux_state_get;
	uint16_t exp_val;
	bool ack;

	i2c_emul = tcpc_emul_get_i2c_emul(emul);
	mux_state = USB_PD_MUX_NONE;

	/* Test fail on standard output config register read */
	tcpc_usb_mux.flags = USB_MUX_FLAG_NOT_TCPC;
	i2c_common_emul_set_read_fail_reg(i2c_emul, TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_mux_get(&tcpc_usb_mux, &mux_state_get), NULL);
	i2c_common_emul_set_read_fail_reg(i2c_emul,
					  I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test fail on standard output config register write */
	tcpc_usb_mux.flags = USB_MUX_FLAG_NOT_TCPC;
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_equal(EC_ERROR_INVAL,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	i2c_common_emul_set_write_fail_reg(i2c_emul,
					   I2C_COMMON_EMUL_NO_FAIL_REG);

	/* Test setting/getting no MUX connection without polarity inverted */
	tcpc_emul_set_reg(emul, TCPC_REG_CONFIG_STD_OUTPUT, 0xff);
	/* Only mux bits should be changed */
	exp_val = 0xf2;
	mux_state = USB_PD_MUX_NONE;
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_false(ack, "Ack from host shouldn't be required");
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_get(&tcpc_usb_mux, &mux_state_get), NULL);
	zassert_equal(mux_state, mux_state_get, "Expected state 0x%x, got 0x%x",
		      mux_state, mux_state_get);

	/* Test setting/getting MUX DP with polarity inverted */
	exp_val = 0xfb;
	mux_state = USB_PD_MUX_DP_ENABLED | USB_PD_MUX_POLARITY_INVERTED;
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_false(ack, "Ack from host shouldn't be required");
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_get(&tcpc_usb_mux, &mux_state_get), NULL);
	zassert_equal(mux_state, mux_state_get, "Expected state 0x%x, got 0x%x",
		      mux_state, mux_state_get);

	/* Test setting/getting MUX USB without polarity inverted */
	exp_val = 0xf6;
	mux_state = USB_PD_MUX_USB_ENABLED;
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_false(ack, "Ack from host shouldn't be required");
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_get(&tcpc_usb_mux, &mux_state_get), NULL);
	zassert_equal(mux_state, mux_state_get, "Expected state 0x%x, got 0x%x",
		      mux_state, mux_state_get);

	/* Test setting/getting MUX USB and DP with polarity inverted */
	exp_val = 0xff;
	mux_state = USB_PD_MUX_USB_ENABLED | USB_PD_MUX_DP_ENABLED |
		    USB_PD_MUX_POLARITY_INVERTED;
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_set(&tcpc_usb_mux, mux_state, &ack), NULL);
	check_tcpc_reg(emul, exp_val, TCPC_REG_CONFIG_STD_OUTPUT);
	zassert_false(ack, "Ack from host shouldn't be required");
	zassert_equal(EC_SUCCESS,
		      tcpci_tcpm_mux_get(&tcpc_usb_mux, &mux_state_get), NULL);
	zassert_equal(mux_state, mux_state_get, "Expected state 0x%x, got 0x%x",
		      mux_state, mux_state_get);
}

void test_suite_tcpci(void)
{
	struct tcpc_config_t tcpc_config_temp;

	/* Copy original tcpc configuration */
	memcpy(&tcpc_config_temp, &tcpc_config[USBC_PORT_C1],
	       sizeof(struct tcpc_config_t));

	tcpc_config[USBC_PORT_C1].bus_type = EC_BUS_TYPE_I2C;
	tcpc_config[USBC_PORT_C1].i2c_info.port = I2C_PORT_USB_C1;
	tcpc_config[USBC_PORT_C1].i2c_info.addr_flags = DT_REG_ADDR(EMUL_LABEL);
	tcpc_config[USBC_PORT_C1].drv = &tcpci_tcpm_drv;

	ztest_test_suite(tcpci,
			 ztest_user_unit_test(test_tcpci_init),
			 ztest_user_unit_test(test_tcpci_release),
			 ztest_user_unit_test(test_tcpci_get_cc),
			 ztest_user_unit_test(test_tcpci_set_cc),
			 ztest_user_unit_test(test_tcpci_set_polarity),
			 ztest_user_unit_test(test_tcpci_set_vconn),
			 ztest_user_unit_test(test_tcpci_set_msg_header),
			 ztest_user_unit_test(test_tcpci_set_rx_detect),
			 ztest_user_unit_test(
				test_tcpci_get_rx_message_raw_rev2),
			 ztest_user_unit_test(test_tcpci_transmit_rev2),
			 ztest_user_unit_test(
				test_tcpci_get_rx_message_raw_rev1),
			 ztest_user_unit_test(test_tcpci_transmit_rev1),
			 ztest_user_unit_test(test_tcpci_alert),
			 ztest_user_unit_test(test_tcpci_alert_rx_message),
			 ztest_user_unit_test(test_tcpci_auto_discharge),
			 ztest_user_unit_test(test_tcpci_drp_toggle),
			 ztest_user_unit_test(test_tcpci_get_chip_info),
			 ztest_user_unit_test(test_tcpci_low_power_mode),
			 ztest_user_unit_test(test_tcpci_set_bist_mode),
			 ztest_user_unit_test(test_tcpc_xfer),
			 ztest_user_unit_test(test_tcpci_debug_accessory),
			 ztest_user_unit_test(test_tcpci_mux_init),
			 ztest_user_unit_test(test_tcpci_mux_enter_low_power),
			 ztest_user_unit_test(test_tcpci_mux_set_get));
	ztest_run_test_suite(tcpci);

	/* Restore original tcpc configuration */
	memcpy(&tcpc_config[USBC_PORT_C1], &tcpc_config_temp,
	       sizeof(struct tcpc_config_t));
}
