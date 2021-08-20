/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr.h>
#include <ztest.h>

#include "common.h"
#include "i2c.h"
#include "stubs.h"
#include "hooks.h"
#include "ec_tasks.h"
#include "emul/emul_bb_retimer.h"
#include "usb_prl_sm.h"

#include "driver/retimer/bb_retimer.h"

#define EMUL_LABEL DT_NODELABEL(usb_c1_bb_retimer_emul)

#define BB_RETIMER_ORD DT_DEP_ORD(EMUL_LABEL)

/** Test init with and without I2C errors. */
static void test_bb_init(void)
{

}

/** Test is retimer fw update capable function. */
static void test_bb_is_fw_update_capable(void)
{
	/* BB retimer is fw update capable */
	zassert_true(bb_usb_retimer.is_retimer_fw_update_capable(), NULL);
}

/** Test is retimer fw update capable function. */
static void test_bb_set_state(void)
{
	struct pd_discovery *disc;
	uint32_t conn, exp_conn;
	struct i2c_emul *emul;
	bool ack_required;

	emul = bb_emul_get(BB_RETIMER_ORD);

	set_test_runner_tid();

	/* Setup emulator fail on write */
	bb_emul_set_write_fail_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);

	/* Test fail on reset register write */
	zassert_equal(-EIO, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
					       USB_PD_MUX_NONE, &ack_required),
		      NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");

	/* Do not fail on write */
	bb_emul_set_write_fail_reg(emul, BB_EMUL_NO_FAIL_REG);

	/* Test none mode set connection state to 0 */
	bb_emul_set_reg(emul, BB_RETIMER_REG_CONNECTION_STATE, 0x12144678);
	zassert_equal(EC_SUCCESS, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
						     USB_PD_MUX_NONE,
						     &ack_required), NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");
	conn = bb_emul_get_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);
	// UFP data role TODO set and test DFP data role
	exp_conn = BB_RETIMER_USB_DATA_ROLE;
	zassert_equal(exp_conn, conn, "Expected state 0x%lx, got 0x%lx",
		      exp_conn, conn);

	/* Test USB3 gen1 mode */
	prl_set_rev(USBC_PORT_C1, TCPC_TX_SOP_PRIME, PD_REV10);
	zassert_equal(EC_SUCCESS, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
						     USB_PD_MUX_USB_ENABLED,
						     &ack_required), NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");
	conn = bb_emul_get_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);
	exp_conn = BB_RETIMER_USB_DATA_ROLE |
		   BB_RETIMER_DATA_CONNECTION_PRESENT |
		   BB_RETIMER_USB_3_CONNECTION;
	zassert_equal(exp_conn, conn, "Expected state 0x%lx, got 0x%lx",
		      exp_conn, conn);

	/* Test USB3 gen2 mode */
	disc = pd_get_am_discovery(USBC_PORT_C1, TCPC_TX_SOP_PRIME);
	disc->identity.product_t1.p_rev20.ss = USB_R20_SS_U31_GEN1_GEN2;
	prl_set_rev(USBC_PORT_C1, TCPC_TX_SOP_PRIME, PD_REV30);
	zassert_equal(EC_SUCCESS, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
						     USB_PD_MUX_USB_ENABLED,
						     &ack_required), NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");
	conn = bb_emul_get_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);
	exp_conn = BB_RETIMER_USB_DATA_ROLE |
		   BB_RETIMER_DATA_CONNECTION_PRESENT |
		   BB_RETIMER_USB_3_CONNECTION |
		   BB_RETIMER_USB_3_SPEED;
	zassert_equal(exp_conn, conn, "Expected state 0x%lx, got 0x%lx",
		      exp_conn, conn);

	/* Test TBT mode */
	zassert_equal(EC_SUCCESS, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
						USB_PD_MUX_TBT_COMPAT_ENABLED,
						&ack_required), NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");
	conn = bb_emul_get_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);
	exp_conn = BB_RETIMER_USB_DATA_ROLE |
		   BB_RETIMER_DATA_CONNECTION_PRESENT |
		   BB_RETIMER_TBT_CONNECTION;
	zassert_equal(exp_conn, conn, "Expected state 0x%lx, got 0x%lx",
		      exp_conn, conn);

	/* Test USB4 mode */
	zassert_equal(EC_SUCCESS, bb_usb_retimer.set(&usb_muxes[USBC_PORT_C1],
						     USB_PD_MUX_USB4_ENABLED,
						     &ack_required), NULL);
	zassert_false(ack_required, "ACK is never required for BB retimer");
	conn = bb_emul_get_reg(emul, BB_RETIMER_REG_CONNECTION_STATE);
	exp_conn = BB_RETIMER_USB_DATA_ROLE |
		   BB_RETIMER_DATA_CONNECTION_PRESENT |
		   BB_RETIMER_USB4_ENABLED;
	zassert_equal(exp_conn, conn, "Expected state 0x%lx, got 0x%lx",
		      exp_conn, conn);
}

DECLARE_DEFERRED(test_bb_set_state);

void test_bb_set_state_call(void)
{
	hook_call_deferred(&test_bb_set_state_data, 0);
	msleep(2000);
}

void test_suite_bb_retimer(void)
{
	ztest_test_suite(bb_retimer,
			 ztest_user_unit_test(test_bb_is_fw_update_capable),
			 ztest_user_unit_test(test_bb_set_state),
			 ztest_user_unit_test(test_bb_init));
	ztest_run_test_suite(bb_retimer);
}
