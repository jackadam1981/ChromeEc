/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB PE module.
 */
#include "battery.h"
#include "common.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "usb_emsg.h"
#include "usb_pe.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_sm_checks.h"

/**
 * STUB Section
 */
struct extended_msg emsg[CONFIG_USB_PD_PORT_COUNT];

const struct svdm_response svdm_rsp = {
	.identity = NULL,
	.svids = NULL,
	.modes = NULL,
};

int battery_design_voltage(int *voltage)
{
	*voltage = 0;
	return 0;
}

enum battery_present battery_is_present(void)
{
	return BP_NO;
}

int battery_design_capacity(int *capacity)
{
	*capacity = 0;
	return 0;
}

int battery_full_charge_capacity(int *capacity)
{
	*capacity = 0;
	return 0;
}

int battery_remaining_capacity(int *capacity)
{
	*capacity = 0;
	return 0;
}

int battery_status(int *status)
{
	return EC_ERROR_UNIMPLEMENTED;
}

int pd_is_vbus_present(int port)
{
	return 0;
}

void pd_request_data_swap(int port)
{}

void pd_request_power_swap(int port)
{}

void pd_request_vconn_swap_off(int port)
{}

void pd_request_vconn_swap_on(int port)
{}

void prl_end_ams(int port)
{}

void prl_execute_hard_reset(int port)
{}

enum pd_rev_type prl_get_rev(int port)
{
	return PD_REV30;
}

void prl_hard_reset_complete(int port)
{}

int prl_is_running(int port)
{
	return 0;
}

void prl_reset(int port)
{}

static enum pd_ctrl_msg_type last_ctrl_msg;
void prl_send_ctrl_msg(int port, enum tcpm_transmit_type type,
	enum pd_ctrl_msg_type msg)
{
	last_ctrl_msg = msg;
}

void prl_send_data_msg(int port, enum tcpm_transmit_type type,
	enum pd_data_msg_type msg)
{}

void prl_send_ext_data_msg(int port, enum tcpm_transmit_type type,
	enum pd_ext_msg_type msg)
{}

void prl_set_rev(int port, enum pd_rev_type rev)
{}

void prl_start_ams(int port)
{}

static int data_role;
int tc_get_data_role(int port)
{
	return data_role;
}
void tc_set_data_role(int port, int role)
{
	data_role = role;
}

static int power_role;
int tc_get_power_role(int port)
{
	return power_role;
}
void tc_set_power_role(int port, int role)
{
	power_role = role;
}

int tc_check_vconn_swap(int port)
{
	return 0;
}

void tc_ctvpd_detected(int port)
{}

void tc_disc_ident_complete(int port)
{}

static int attached_snk;
int tc_is_attached_snk(int port)
{
	return attached_snk;
}

static int attached_src;
int tc_is_attached_src(int port)
{
	return attached_src;
}

int tc_is_vconn_src(int port)
{
	return 0;
}

void tc_hard_reset(int port)
{}

void tc_partner_dr_data(int port, int en)
{}

void tc_partner_dr_power(int port, int en)
{}

void tc_partner_extpower(int port, int en)
{}

void tc_partner_usb_comm(int port, int en)
{}

void tc_pd_connection(int port, int en)
{}

void tc_pr_swap_complete(int port)
{}

void tc_prs_snk_src_assert_rp(int port)
{}

void tc_prs_src_snk_assert_rd(int port)
{}

void tc_set_timeout(int port, uint64_t timeout)
{}

void tc_start_error_recovery(int port)
{}

void tc_snk_power_off(int port)
{}


/**
 * Test section
 */
/* PE Fast Role Swap */
static int test_pe_frs(void)
{
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(pe_is_running(PORT0));

	attached_snk = 1;
	attached_src = 0;
	pe_set_flag(PORT0, PE_FLAGS_EXPLICIT_CONTRACT);
	set_state_pe(PORT0, PE_SNK_READY);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_SNK_READY);

	pe_got_frs_signal(PORT0);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_SIGNALED));

	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_SEND_SWAP);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));
	TEST_ASSERT(!pe_chk_flag(PORT0, PE_FLAGS_EXPLICIT_CONTRACT));

	pe_run(PORT0, 0, 1);
	TEST_ASSERT(last_ctrl_msg == PD_CTRL_FR_SWAP);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_SEND_SWAP);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	emsg[PORT0].header = PD_HEADER(PD_CTRL_ACCEPT, 0, 0, 0, 0, 0, 0);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_SEND_SWAP);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	pe_set_flag(PORT0, PE_FLAGS_MSG_RECEIVED);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(!pe_chk_flag(PORT0, PE_FLAGS_MSG_RECEIVED));
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_TRANSITION_TO_OFF);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	emsg[PORT0].header = PD_HEADER(PD_CTRL_PS_RDY, 0, 0, 0, 0, 0, 0);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_TRANSITION_TO_OFF);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	pe_set_flag(PORT0, PE_FLAGS_MSG_RECEIVED);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(!pe_chk_flag(PORT0, PE_FLAGS_MSG_RECEIVED));
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_ASSERT_RP);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	attached_src = 1;
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_SOURCE_ON);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	/* Move the time to be after our wait time. */
	force_time((timestamp_t)(get_time().val +
				 PD_POWER_SUPPLY_TURN_ON_DELAY));

	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_PRS_SNK_SRC_SOURCE_ON);
	TEST_ASSERT(pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));
	TEST_ASSERT(last_ctrl_msg == PD_CTRL_PS_RDY);

	pe_set_flag(PORT0, PE_FLAGS_TX_COMPLETE);
	pe_run(PORT0, 0, 1);
	TEST_ASSERT(get_state_pe(PORT0) == PE_SRC_STARTUP);
	TEST_ASSERT(!pe_chk_flag(PORT0, PE_FLAGS_FAST_ROLE_SWAP_PATH));

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	RUN_TEST(test_pe_frs);

	/* Do basic state machine sanity checks last. */
	RUN_TEST(test_pe_no_parent_cycles);
	RUN_TEST(test_pe_no_empty_state);

	test_print_result();
}
