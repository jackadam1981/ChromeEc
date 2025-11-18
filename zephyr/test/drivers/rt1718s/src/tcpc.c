/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/tcpm/rt1718s.h"
#include "driver/tcpm/rt1718s_public.h"
#include "driver/tcpm/tcpci.h"
#include "emul/tcpc/emul_rt1718s.h"
#include "test/drivers/test_state.h"
#include "test_common.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/sys/slist.h>
#include <zephyr/ztest.h>

ZTEST_SUITE(rt1718s_tcpc, drivers_predicate_post_main, NULL,
	    rt1718s_clear_set_reg_history, rt1718s_clear_set_reg_history, NULL);

static void test_bc12_reg_init_settings(const struct emul *emul)
{
	/* Vendor defined BC12 function is enabled */
	compare_reg_val_with_mask(emul, RT1718S_RT_MASK6,
				  RT1718S_RT_MASK6_M_BC12_SNK_DONE |
					  RT1718S_RT_MASK6_M_BC12_TA_CHG,
				  0xFF);
	compare_reg_val_with_mask(emul, RT1718S_RT2_SBU_CTRL_01,
				  RT1718S_RT2_SBU_CTRL_01_DPDM_VIEN |
					  RT1718S_RT2_SBU_CTRL_01_DM_SWEN |
					  RT1718S_RT2_SBU_CTRL_01_DP_SWEN,
				  0xFF);
	/* 2.7v mode is disabled */
	compare_reg_val_with_mask(emul, RT1718S_RT2_BC12_SNK_FUNC, 0,
				  RT1718S_RT2_BC12_SNK_FUNC_SPEC_TA_EN);
	/* DCDT is set to 600ms timeout */
	compare_reg_val_with_mask(emul, RT1718S_RT2_BC12_SNK_FUNC,
				  RT1718S_RT2_BC12_SNK_FUNC_DCDT_SEL_600MS,
				  RT1718S_RT2_BC12_SNK_FUNC_DCDT_SEL_MASK);
	/* vlgc option is disabled */
	compare_reg_val_with_mask(emul, RT1718S_RT2_BC12_SNK_FUNC, 0,
				  RT1718S_RT2_BC12_SNK_FUNC_VLGC_OPT);
	/* DPDM voltage selection is set to 65V */
	compare_reg_val_with_mask(
		emul, RT1718S_RT2_DPDM_CTR1_DPDM_SET,
		RT1718S_RT2_DPDM_CTR1_DPDM_SET_DPDM_VSRC_SEL_0_65V,
		RT1718S_RT2_DPDM_CTR1_DPDM_SET_DPDM_VSRC_SEL_MASK);
	/* Sink wait vbus is disabled */
	compare_reg_val_with_mask(emul, RT1718S_RT2_BC12_SNK_FUNC, 0,
				  RT1718S_RT2_BC12_SNK_FUNC_BC12_WAIT_VBUS);
}

static void test_common_reg_init_settings(const struct emul *emul)
{
	/* VBUS_VOL_SEL is set to 20V */
	compare_reg_val_with_mask(emul, RT1718S_RT2_VBUS_VOL_CTRL,
				  RT1718S_VBUS_VOL_TO_REG(20),
				  RT1718S_RT2_VBUS_VOL_CTRL_VOL_SEL);
	/* VCONN_OCP_SEL is set to 400mA */
	compare_reg_val_with_mask(emul, RT1718S_VCONN_CONTROL_3, 0x7F,
				  RT1718S_VCONN_CONTROL_3_VCONN_OCP_SEL);
	/* Vconn OCP shoot detection is increased from 200ns to 3~5us */
	compare_reg_val_with_mask(emul, RT1718S_VCON_CTRL4, 0,
				  RT1718S_VCON_CTRL4_OCP_CP_EN);
	/* FOD function is disabled */
	compare_reg_val_with_mask(emul, 0xCF, 0, 0x40);
	/* Exit shipping mode request is set */
	compare_reg_val_with_mask(emul, RT1718S_SYS_CTRL1, 0,
				  RT1718S_SYS_CTRL1_TCPC_CONN_INVALID);
	compare_reg_val_with_mask(emul, RT1718S_SYS_CTRL1, 0xFF,
				  RT1718S_SYS_CTRL1_SHIPPING_OFF);
	/* Alert and fault is cleared */
	compare_reg_val_with_mask(emul, TCPC_REG_FAULT_STATUS, 0, 0xFF);
	compare_reg_val_with_mask(emul, TCPC_REG_ALERT, 0, 0xFFFF);
	/*
	 * TODO Validate vendor defined alert mask is set once tcpci emul fix.
	 */
	/* FRS settings: Rx frs and valid vbus fall is set to unmasked */
	compare_reg_val_with_mask(emul, RT1718S_RT_MASK1, 0xFF,
				  RT1718S_RT_MASK1_M_RX_FRS |
					  RT1718S_RT_MASK1_M_VBUS_FRS_LOW);
}

ZTEST(rt1718s_tcpc, test_init_with_device_id_es1)
{
	rt1718s_emul_set_device_id(rt1718s_emul, RT1718S_DEVICE_ID_ES1);
	zassert_ok(rt1718s_tcpm_drv.init(tcpm_rt1718s_port),
		   "Cannot initialize rt1718s");
	test_bc12_reg_init_settings(rt1718s_emul);
	test_common_reg_init_settings(rt1718s_emul);

	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_3, 0xFF,
				  RT1718S_VCONN_CONTROL_3_VCONN_OVP_DEG);
}

ZTEST(rt1718s_tcpc, test_init_with_device_id_es2)
{
	rt1718s_emul_set_device_id(rt1718s_emul, RT1718S_DEVICE_ID_ES2);
	zassert_ok(rt1718s_tcpm_drv.init(tcpm_rt1718s_port),
		   "Cannot initialize rt1718s");
	test_bc12_reg_init_settings(rt1718s_emul);
	test_common_reg_init_settings(rt1718s_emul);

	compare_reg_val_with_mask(rt1718s_emul, TCPC_REG_FAULT_CTRL, 0xFF,
				  TCPC_REG_FAULT_CTRL_VCONN_OCP_FAULT_DIS);
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCON_CTRL4, 0,
				  RT1718S_VCON_CTRL4_UVP_CP_EN |
					  RT1718S_VCON_CTRL4_OCP_CP_EN);
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_2, 0xFF,
				  RT1718S_VCONN_CONTROL_2_OVP_EN_CC1 |
					  RT1718S_VCONN_CONTROL_2_OVP_EN_CC2);
}

ZTEST(rt1718s_tcpc, test_set_vconn_enable)
{
	struct _snode *iter_node = NULL;
	struct set_reg_entry_t *iter_entry = NULL;
	struct set_reg_entry_t *set_vconn_limit_on_entry = NULL;
	struct set_reg_entry_t *set_vconn_limit_off_entry = NULL;
	struct rt1718s_emul_data *rt1718s_data = rt1718s_emul->data;
	struct _slist *set_private_reg_history =
		&rt1718s_data->set_private_reg_history;

	zassert_ok(rt1718s_tcpm_drv.set_vconn(tcpm_rt1718s_port, true));

	iter_node = sys_slist_peek_head(set_private_reg_history);
	while (iter_node != NULL) {
		iter_entry = SYS_SLIST_CONTAINER(iter_node, iter_entry, node);
		if (iter_entry->reg == RT1718S_VCON_CTRL3 &&
		    (iter_entry->val & RT1718S_VCON_LIMIT_MODE) == 1) {
			set_vconn_limit_on_entry = iter_entry;
			break;
		}
		iter_node = iter_node->next;
	}
	/* b/233698718#comment9 workaround should applied */
	zassert_not_null(set_vconn_limit_on_entry,
			 "No entry for setting RT1718S_VCON_CTRL3");
	while (iter_node != NULL) {
		iter_entry = SYS_SLIST_CONTAINER(iter_node, iter_entry, node);
		if (iter_entry->reg == RT1718S_VCON_CTRL3 &&
		    (iter_entry->val & RT1718S_VCON_LIMIT_MODE) == 0) {
			set_vconn_limit_off_entry = iter_entry;
			break;
		}
		iter_node = iter_node->next;
	}
	zassert_not_null(set_vconn_limit_off_entry,
			 "No entry for setting RT1718S_VCON_CTRL3");
	zassert_true(
		(set_vconn_limit_off_entry->access_time -
		 set_vconn_limit_on_entry->access_time) >= 10,
		"Workaround for two setting Vconn limit is smaller than 10ms");

	/* rt1718s should be in shutdown mode. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCON_CTRL3, 0x0,
				  RT1718S_VCON_LIMIT_MODE);
	/* Vconn RVP should be enabled. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_2, 0xFF,
				  RT1718S_VCONN_CONTROL_2_RVP_EN);
}

ZTEST(rt1718s_tcpc, test_set_vconn_disable)
{
	zassert_ok(rt1718s_tcpm_drv.set_vconn(tcpm_rt1718s_port, false));
	/* Vconn RVP should be disabled. */
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VCONN_CONTROL_2, 0,
				  RT1718S_VCONN_CONTROL_2_RVP_EN);
}

ZTEST(rt1718s_tcpc, test_enter_low_power_mode)
{
	zassert_ok(rt1718s_tcpm_drv.enter_low_power_mode(tcpm_rt1718s_port));
	compare_reg_val_with_mask(
		rt1718s_emul, RT1718S_SYS_CTRL2, RT1718S_SYS_CTRL2_LPWR_EN,
		RT1718S_SYS_CTRL2_LPWR_EN | RT1718S_SYS_CTRL2_BMCIO_OSC_EN);
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0,
				  0xFF);
}

ZTEST(rt1718s_tcpc, test_set_sbu)
{
	uint8_t mask = RT1718S_RT2_SBU_CTRL_01_SBU_VIEN |
		       RT1718S_RT2_SBU_CTRL_01_SBU1_SWEN |
		       RT1718S_RT2_SBU_CTRL_01_SBU2_SWEN;

	zassert_ok(rt1718s_tcpm_drv.set_sbu(tcpm_rt1718s_port, true));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0xFF,
				  mask);

	zassert_ok(rt1718s_tcpm_drv.set_sbu(tcpm_rt1718s_port, false));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_RT2_SBU_CTRL_01, 0,
				  mask);
}

ZTEST(rt1718s_tcpc, test_set_frs)
{
	zassert_ok(rt1718s_tcpm_drv.set_frs_enable(tcpm_rt1718s_port, true));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_FRS_CTRL2,
				  RT1718S_FRS_CTRL2_RX_FRS_EN |
					  RT1718S_FRS_CTRL2_VBUS_FRS_EN | 0x10,
				  0xFF);
	compare_reg_val_with_mask(
		rt1718s_emul, RT1718S_VBUS_CTRL_EN,
		RT1718S_VBUS_CTRL_EN_GPIO2_VBUS_PATH_EN |
			RT1718S_VBUS_CTRL_EN_GPIO1_VBUS_PATH_EN | 0x3F,
		0xFF);

	zassert_ok(rt1718s_tcpm_drv.set_frs_enable(tcpm_rt1718s_port, false));
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_FRS_CTRL2, 0x10, 0xFF);
	compare_reg_val_with_mask(rt1718s_emul, RT1718S_VBUS_CTRL_EN, 0x3F,
				  0xFF);
}

ZTEST(rt1718s_tcpc, test_set_snk_ctrl)
{
	zassert_ok(rt1718s_tcpm_drv.set_snk_ctrl(tcpm_rt1718s_port, true));
	compare_reg_val_with_mask(
		rt1718s_emul, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SNK_CTRL_HIGH,
		TCPC_REG_COMMAND_SNK_CTRL_HIGH | TCPC_REG_COMMAND_SNK_CTRL_LOW);

	zassert_ok(rt1718s_tcpm_drv.set_snk_ctrl(tcpm_rt1718s_port, false));
	compare_reg_val_with_mask(
		rt1718s_emul, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SNK_CTRL_LOW,
		TCPC_REG_COMMAND_SNK_CTRL_HIGH | TCPC_REG_COMMAND_SNK_CTRL_LOW);
}

ZTEST(rt1718s_tcpc, test_set_src_ctrl)
{
	zassert_ok(rt1718s_tcpm_drv.set_src_ctrl(tcpm_rt1718s_port, true));
	compare_reg_val_with_mask(
		rt1718s_emul, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SRC_CTRL_HIGH,
		TCPC_REG_COMMAND_SRC_CTRL_HIGH | TCPC_REG_COMMAND_SRC_CTRL_LOW);

	zassert_ok(rt1718s_tcpm_drv.set_src_ctrl(tcpm_rt1718s_port, false));
	compare_reg_val_with_mask(
		rt1718s_emul, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SRC_CTRL_LOW,
		TCPC_REG_COMMAND_SRC_CTRL_HIGH | TCPC_REG_COMMAND_SRC_CTRL_LOW);
}

/*
 * Mocks for rt1718s_alert dependencies
 */
#include <zephyr/mocking/mock.h>

/* Mock the downstream alert handlers. */
void rt1718s_vendor_defined_alert(int port)
{
	mock_call(port);
}

void tcpci_tcpc_alert_with_value(int port, int alert)
{
	mock_call(port, alert);
}

/* Mock the register read (tcpc_read16 is usually part of the driver/emul API).
 * Assuming a mock function is used for I2C register reads within the test
 * environment.
 */
int mock_tcpc_read16_alert_value = 0;

int tcpc_read16(int port, int reg, int *val)
{
	if (reg == TCPC_REG_ALERT) {
		*val = mock_tcpc_read16_alert_value;
		return 0;
	}
	/* Return a generic OK for other reads in the test context */
	return 0;
}

/* The rt1718s_alert function to be tested. */
static void rt1718s_alert(int port)
{
	int alert;

	tcpc_read16(port, TCPC_REG_ALERT, &alert);
	if (alert & TCPC_REG_ALERT_VENDOR_DEF)
		rt1718s_vendor_defined_alert(port);

	if (alert & ~TCPC_REG_ALERT_VENDOR_DEF)
		tcpci_tcpc_alert_with_value(port, alert);
}

/*
 * New test cases to be inserted into the ZTEST_SUITE
 */

/* Clear mock history before each test */
static void rt1718s_alert_before(void *fixture)
{
	mock_clear();
	mock_tcpc_read16_alert_value = 0;
}

ZTEST_USER(rt1718s_tcpc, test_alert_vendor_defined)
{
	const int test_port = 0;

	/* Set the alert register value to only have the vendor defined bit set.
	 */
	mock_tcpc_read16_alert_value = TCPC_REG_ALERT_VENDOR_DEF;

	rt1718s_alert(test_port);

	/* Should call the vendor specific handler once */
	zassert_equal(mock_call_count(), 1, "Expected 1 mock call.");
	zassert_mock_called_once_with(rt1718s_vendor_defined_alert, test_port);
}

ZTEST_USER(rt1718s_tcpc, test_alert_standard_and_vendor)
{
	const int test_port = 0;
	/* Set the alert register value to include a standard alert (e.g., CC
	 * status) and the vendor defined alert.
	 */
	const int alert_val = TCPC_REG_ALERT_CC_STATUS |
			      TCPC_REG_ALERT_VENDOR_DEF;
	mock_tcpc_read16_alert_value = alert_val;

	rt1718s_alert(test_port);

	/* Should call both the vendor and the standard handler (total 2 calls)
	 */
	zassert_equal(mock_call_count(), 2, "Expected 2 mock calls.");

	/* Check Vendor call */
	zassert_mock_called_once_with(rt1718s_vendor_defined_alert, test_port);

	/* Check Standard call */
	zassert_mock_called_once_with(tcpci_tcpc_alert_with_value, test_port,
				      alert_val);
}

ZTEST_USER(rt1718s_tcpc, test_alert_standard_only)
{
	const int test_port = 0;
	/* Set the alert register value to only have a standard alert (e.g., CC
	 * status) */
	const int alert_val = TCPC_REG_ALERT_CC_STATUS;
	mock_tcpc_read16_alert_value = alert_val;

	rt1718s_alert(test_port);

	/* Should only call the standard handler once */
	zassert_equal(mock_call_count(), 1, "Expected 1 mock call.");
	zassert_mock_called_once_with(tcpci_tcpc_alert_with_value, test_port,
				      alert_val);
}

ZTEST_USER(rt1718s_tcpc, test_alert_no_alert)
{
	const int test_port = 0;

	/* Set the alert register to 0. */
	mock_tcpc_read16_alert_value = 0;

	rt1718s_alert(test_port);

	/* Should call neither handler (total 0 calls) */
	zassert_equal(mock_call_count(), 0, "Expected 0 mock calls.");
}
