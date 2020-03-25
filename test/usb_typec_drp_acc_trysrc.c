/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB Type-C VPD and CTVPD module.
 */
#include "charge_manager.h"
#include "mock/tcpc_mock.h"
#include "mock/usb_mux_mock.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "usb_sm_checks.h"
#include "hooks.h"

#define PORT0 0

enum usb_tc_state {
	/* Normal States */
	TC_DISABLED,
	TC_ERROR_RECOVERY,
	TC_UNATTACHED_SNK,
	TC_ATTACH_WAIT_SNK,
	TC_ATTACHED_SNK,
	TC_UNORIENTED_DBG_ACC_SRC,
	TC_DBG_ACC_SNK,
	TC_UNATTACHED_SRC,
	TC_ATTACH_WAIT_SRC,
	TC_ATTACHED_SRC,
	TC_TRY_SRC,
	TC_TRY_WAIT_SNK,
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	TC_DRP_AUTO_TOGGLE,
#endif
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	TC_LOW_POWER_MODE,
#endif
#ifdef CONFIG_USB_PE_SM
	TC_CT_UNATTACHED_SNK,
	TC_CT_ATTACHED_SNK,
#endif
	/* Super States */
	TC_CC_OPEN,
	TC_CC_RD,
	TC_CC_RP,
};

const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int pd_check_vconn_swap(int port)
{
	return 1;
}

void pd_request_vconn_swap_off(int port)
{}

void pd_request_vconn_swap_on(int port)
{}

/* Install Mock TCPC and MUX drivers */
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.drv = &mock_tcpc_driver,
	},
};

const struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.driver = &mock_usb_mux_driver,
	}
};

void charge_manager_set_ceil(int port, enum ceil_requestor requestor, int ceil)
{
	/* Do Nothing, but needed for linking */
}

__maybe_unused static int test_mux_con_dis_as_src(void)
{
	/* Update CC lines send state machine event to process */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_RD;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);
	pd_set_dual_role(0, PD_DRP_TOGGLE_ON);

	/* This wait trainsitions through AttachWait.SRC then Attached.SRC */
	task_wait_event(SECOND);

	/* We are in Attached.SRC now */
	TEST_EQ(mock_usb_mux.state, USB_PD_MUX_USB_ENABLED, "%d");
	TEST_EQ(mock_usb_mux.num_set_calls, 1, "%d");

	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);

	/* This wait will go through TryWait.SNK then to Unattached.SNK */
	task_wait_event(10 * SECOND);

	/* We are in Unattached.SNK. The mux should have detached */
	TEST_EQ(mock_usb_mux.state, USB_PD_MUX_NONE, "%d");
	TEST_EQ(mock_usb_mux.num_set_calls, 2, "%d");

	return EC_SUCCESS;
}

__maybe_unused static int test_mux_con_dis_as_snk(void)
{
	/* Update CC lines send state machine event to process */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_RP_3_0;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.vbus_level = 1;
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);

	/* This wait will go through AttachWait.SNK to Attached.SNK */
	task_wait_event(5 * SECOND);

	/* We are in Attached.SNK now */
	TEST_EQ(mock_usb_mux.state, USB_PD_MUX_USB_ENABLED, "%d");
	TEST_EQ(mock_usb_mux.num_set_calls, 1, "%d");

	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.vbus_level = 0;
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);

	/* This wait will go through TryWait.SNK then to Unattached.SNK */
	task_wait_event(10 * SECOND);

	/* We are in Unattached.SNK. The mux should have detached */
	TEST_EQ(mock_usb_mux.state, USB_PD_MUX_NONE, "%d");
	TEST_EQ(mock_usb_mux.num_set_calls, 2, "%d");

	return EC_SUCCESS;
}

__maybe_unused static int test_power_role_set(void)
{
	/* Print out header changes for easier debugging */
	mock_tcpc.should_print_header_changes = true;

	/* Update CC lines send state machine event to process */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_RD;
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);
	task_wait_event(10 * SECOND);

	/* We are in Attached.SRC now */
	TEST_EQ(mock_tcpc.power_role, PD_ROLE_SOURCE, "%d");
	TEST_EQ(mock_tcpc.data_role, PD_ROLE_DFP, "%d");

	/*
	 * We allow 2 separate calls to update the header since power and data
	 * role updates can be separate calls depending on the state is came
	 * from.
	 */
	TEST_LE(mock_tcpc.num_calls_to_set_header, 2, "%d");

	return EC_SUCCESS;
}

__maybe_unused static int test_toggle_on(void)
{
	/* Print out header changes for easier debugging */
	mock_tcpc.should_print_header_changes = true;

	task_wait_event(10 * SECOND);
	ccprints("state %s", pd_get_task_state_name(PORT0));
	TEST_ASSERT(pd_get_task_state(PORT0) == TC_LOW_POWER_MODE);

	hook_notify(HOOK_CHIPSET_STARTUP);
	task_wait_event(5 * MSEC);
	hook_notify(HOOK_CHIPSET_RESUME);
	task_wait_event(10 * SECOND);

	return EC_SUCCESS;
}

/* Reset the mocks before each test */
void before_test(void)
{
	mock_usb_mux_reset();
	mock_tcpc_reset();
}

void after_test(void)
{
	/* Disconnect any CC lines */
	mock_tcpc.cc1 = TYPEC_CC_VOLT_OPEN;
	mock_tcpc.cc2 = TYPEC_CC_VOLT_OPEN;
	ccprints("after_test");
	task_set_event(TASK_ID_PD_C0, PD_EVENT_CC, 0);
}

void run_test(void)
{
	test_reset();

	/* Ensure that PD task initializes its state machine */
	task_wake(TASK_ID_PD_C0);
	task_wait_event(5 * MSEC);

	RUN_TEST(test_toggle_on);


	test_print_result();
}
