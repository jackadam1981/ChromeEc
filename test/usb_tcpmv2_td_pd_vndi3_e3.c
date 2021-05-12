/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mock/tcpci_i2c_mock.h"
#include "task.h"
#include "tcpm/tcpci.h"
#include "test_util.h"
#include "timer.h"
#include "usb_tcpmv2_compliance.h"
#include "usb_tc_sm.h"

uint32_t vdo = VDO(USB_SID_PD, 1,
		   VDO_SVDM_VERS(VDM_VER20) |
		   CMD_DISCOVER_IDENT);


/*****************************************************************************
 * TD.PD.VNDI3.E3.VDM Identity
 *
 * Description:
 *	This test verifies that the VDM Information is as specified in the
 *	vendor-supplied information.
 */
static int td_pd_vndi3_e3(enum pd_data_role data_role)
{
	partner_set_pd_rev(PD_REV30);

	TEST_EQ(tcpci_startup(), EC_SUCCESS, "%d");

	/*
	 * a) Run PROC.PD.E1 Bring-up according to the UUT role.
	 */
	TEST_EQ(proc_pd_e1(data_role, INITIAL_AND_ALREADY_ATTACHED),
		EC_SUCCESS, "%d");

	/*
	 * Make sure we are idle. Reject everything that is pending
	 */
	TEST_EQ(handle_attach_expected_msgs(data_role), EC_SUCCESS, "%d");

	/*
	 * b) Tester executes a Discover Identity exchange
	 */
	partner_send_msg(PD_MSG_SOP, PD_DATA_VENDOR_DEF,
			 1, 0, &vdo);

	/*
	 * c) If the UUT is not a cable and if Responds_To_Discov_SOP is set to
	 * No, the tester checks that hte UUT replies Not_Supported.  The test
	 * stops here in this case.
	 */
	TEST_EQ(verify_tcpci_transmit(TCPC_TX_SOP, PD_CTRL_NOT_SUPPORTED, 0),
		EC_SUCCESS, "%d");
	mock_set_alert(TCPC_REG_ALERT_TX_SUCCESS);

	/*
	 * TODO: Items d)-i) could be verified if the unit tests are configured
	 * to reply to Identity messages.
	 */
	return EC_SUCCESS;
}
int test_td_pd_vndi3_e3_dfp(void)
{
	return td_pd_vndi3_e3(PD_ROLE_DFP);
}
int test_td_pd_vndi3_e3_ufp(void)
{
	return td_pd_vndi3_e3(PD_ROLE_UFP);
}
