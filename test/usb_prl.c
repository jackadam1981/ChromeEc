/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test USB Protocol Layer module.
 */
#include "common.h"
#include "crc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "tcpm.h"
#include "usb_pe_sm.h"
#include "usb_pd.h"
#include "usb_pd_test_util.h"
#include "usb_prl_sm.h"
#include "util.h"

#define PORT0 0
#define PORT1 1

static struct pd_prl {
	int rev;
	int pd_enable;
	int power_role;
	int data_role;
	int msg_tx_id;
	int msg_rx_id;

	int mock_pe_message_sent;
	int mock_pe_error;
	int mock_pe_hard_reset_sent;
	int mock_pe_got_hard_reset;
	int mock_pe_pass_up_message;
	int mock_got_soft_reset;
} pd_port[CONFIG_USB_PD_PORT_COUNT];

static void init_port(int port, int rev)
{
	pd_port[port].rev = rev;
	pd_port[port].pd_enable = 0;
	pd_port[port].power_role = PD_ROLE_SINK;
	pd_port[port].data_role = PD_ROLE_UFP;
	pd_port[port].msg_tx_id = 0;
	pd_port[port].msg_rx_id = 0;
	tcpm_init(port);
	tcpm_set_polarity(port, 0);
	tcpm_set_rx_enable(port, 0);
}

void inc_tx_id(int port)
{
	pd_port[port].msg_tx_id = (pd_port[port].msg_tx_id + 1) & 7;
}

static void simulate_rx_msg(int port, uint16_t header, int cnt,
							const uint32_t *data)
{
	int i;

	pd_test_rx_set_preamble(port, 1);
	pd_test_rx_msg_append_sop(port);
	pd_test_rx_msg_append_short(port, header);

	crc32_init();
	crc32_hash16(header);

	for (i = 0; i < cnt; ++i) {
		pd_test_rx_msg_append_word(port, data[i]);
		crc32_hash32(data[i]);
	}

	pd_test_rx_msg_append_word(port, crc32_result());

	pd_test_rx_msg_append_eop(port);
	pd_test_rx_msg_append_last_edge(port);

	pd_simulate_rx(port);
}

static void simulate_goodcrc(int port, int role, int id)
{
	simulate_rx_msg(port, PD_HEADER(PD_CTRL_GOOD_CRC, role, role, id, 0,
						pd_port[port].rev, 0), 0, NULL);
}

static void enable_prl(int port, int en)
{
	tcpm_set_rx_enable(port, en);

	pd_port[port].pd_enable = en;
	pd_port[port].msg_tx_id = 0;
	pd_port[port].msg_rx_id = 0;

	/* Init PRL */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(10 * MSEC);

	prl_set_rev(port, pd_port[port].rev);
}

static void cycle_through_state_machine(int port, unsigned int num,
							unsigned int time)
{
	int i;

	for (i = 0; i < num; i++) {
		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(time);
	}
}


int tc_get_power_role(int port)
{
	return pd_port[port].power_role;
}

int tc_get_data_role(int port)
{
	return pd_port[port].data_role;
}

void pe_report_error(int port, enum pe_error e)
{
	pd_port[port].mock_pe_error = e;
}

void pe_got_hard_reset(int port)
{
	pd_port[port].mock_pe_got_hard_reset = 1;
}

void pe_pass_up_message(int port)
{
	pd_port[port].mock_pe_pass_up_message = 1;
}

void pe_message_sent(int port)
{
	pd_port[port].mock_pe_message_sent = 1;
}

void pe_hard_reset_sent(int port)
{
	pd_port[port].mock_pe_hard_reset_sent = 1;
}

void pe_got_soft_reset(int port)
{
	pd_port[port].mock_got_soft_reset = 1;
}

static int test_initial_states(void)
{
	int port = PORT0;

	enable_prl(port, 1);

	TEST_ASSERT(get_prl_tx_state_id(port) ==
				PRL_TX_WAIT_FOR_MESSAGE_REQUEST);
	TEST_ASSERT(get_rch_state_id(port) ==
				RCH_WAIT_FOR_MESSAGE_FROM_PROTOCOL_LAYER);
	TEST_ASSERT(get_tch_state_id(port) ==
				TCH_WAIT_FOR_MESSAGE_REQUEST_FROM_PE);
	TEST_ASSERT(get_prl_hr_state_id(port) ==
				PRL_HR_WAIT_FOR_PE_HARD_RESET_COMPLETE);

	return EC_SUCCESS;
}

static int test_ctrl_msg_request_received(void)
{
	int i;
	int port = PORT0;

	enable_prl(port, 1);

	/*
	 * TEST: Control message transmission and tx_id increment
	 */
	for (i = 0; i < 10; i++) {
		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(40 * MSEC);

		TEST_ASSERT(get_prl_tx_state_id(port) ==
					PRL_TX_WAIT_FOR_MESSAGE_REQUEST);

		pd_port[port].mock_pe_message_sent = 0;
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
		task_wait_event(40 * MSEC);

		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(30 * MSEC);

		simulate_goodcrc(port, pd_port[port].power_role,
						pd_port[port].msg_tx_id);
		inc_tx_id(port);

		cycle_through_state_machine(port, 3, 10 * MSEC);

		TEST_ASSERT(pd_port[port].mock_pe_message_sent);
	}

	enable_prl(port, 0);

	return EC_SUCCESS;
}

static int test_ctrl_msg_request_received_with_retry_and_fail(void)
{
	int i;
	int port = PORT0;

	enable_prl(port, 1);

	/*
	 * TEST: Control message transmission fail with retry
	 */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_prl_tx_state_id(port) ==
			PRL_TX_WAIT_FOR_MESSAGE_REQUEST);

	pd_port[port].mock_pe_error = -1;
	pd_port[port].mock_pe_message_sent = 0;

	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	task_wait_event(30 * MSEC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	simulate_goodcrc(port, pd_port[port].power_role,
					pd_port[port].msg_tx_id);

	/* Do not increment tx_id so phy layer will not transmit message */

	cycle_through_state_machine(port, 3, 10 * MSEC);

	TEST_ASSERT(pd_port[port].mock_pe_message_sent);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_prl_tx_state_id(port) ==
					PRL_TX_WAIT_FOR_MESSAGE_REQUEST);

	pd_port[port].mock_pe_message_sent = 0;
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	task_wait_event(30 * MSEC);

	for (i = 0; i < N_RETRY_COUNT + 1; i++) {
		cycle_through_state_machine(port, 8, 10 * MSEC);

		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(PD_T_TCPC_TX_TIMEOUT);

		TEST_ASSERT(pd_port[port].mock_pe_message_sent == 0);
		if (i == N_RETRY_COUNT)
			TEST_ASSERT(pd_port[port].mock_pe_error == ERR_PRL_TX);
		else
			TEST_ASSERT(pd_port[port].mock_pe_error < 0);
	}

	enable_prl(port, 0);

	return EC_SUCCESS;
}

static int test_ctrl_msg_request_received_with_retry_and_success(void)
{
	int i;
	int port = PORT0;

	enable_prl(port, 1);

	/*
	 * TEST: Control message transmission fail with retry
	 */
	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_prl_tx_state_id(port) ==
				PRL_TX_WAIT_FOR_MESSAGE_REQUEST);

	pd_port[port].mock_pe_error = -1;
	pd_port[port].mock_pe_message_sent = 0;

	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	task_wait_event(40 * MSEC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	simulate_goodcrc(port, pd_port[port].power_role,
						pd_port[port].msg_tx_id);

	/* Do not increment tx_id. */

	cycle_through_state_machine(port, 3, 10 * MSEC);

	TEST_ASSERT(pd_port[port].mock_pe_message_sent);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(40 * MSEC);

	TEST_ASSERT(get_prl_tx_state_id(port) ==
				PRL_TX_WAIT_FOR_MESSAGE_REQUEST);

	pd_port[port].mock_pe_message_sent = 0;
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	task_wait_event(30 * MSEC);

	task_wake(PD_PORT_TO_TASK_ID(port));
	task_wait_event(30 * MSEC);

	for (i = 0; i < N_RETRY_COUNT + 1; i++) {
		if (i == N_RETRY_COUNT)
			inc_tx_id(port);

		simulate_goodcrc(port, pd_port[port].power_role,
						pd_port[port].msg_tx_id);

		cycle_through_state_machine(port, 8, 10 * MSEC);

		task_wake(PD_PORT_TO_TASK_ID(port));
		task_wait_event(PD_T_TCPC_TX_TIMEOUT);

		if (i == N_RETRY_COUNT)
			TEST_ASSERT(pd_port[port].mock_pe_message_sent);
		else
			TEST_ASSERT(pd_port[port].mock_pe_message_sent == 0);
		TEST_ASSERT(pd_port[port].mock_pe_error < 0);
	}

	enable_prl(port, 0);

	return EC_SUCCESS;
}

int pd_task(void *u)
{
	int port = PORT0;
	int evt;

	while (1) {
		evt = task_wait_event(-1);

		tcpc_run(port, evt);
		protocol_layer(port, evt, pd_port[port].pd_enable);
	}

	return EC_SUCCESS;
}

void run_test(void)
{
	test_reset();

	/* Test PD 2.0 Protocol */
	init_port(PORT0, PD_REV20);
	RUN_TEST(test_initial_states);
	RUN_TEST(test_ctrl_msg_request_received);
	RUN_TEST(test_ctrl_msg_request_received_with_retry_and_fail);
	RUN_TEST(test_ctrl_msg_request_received_with_retry_and_success);

	/* TODO(shurst): More PD 2.0 Tests */

	/* Test PD 3.0 Protocol */
	init_port(PORT0, PD_REV30);
	RUN_TEST(test_initial_states);
	RUN_TEST(test_ctrl_msg_request_received);
	RUN_TEST(test_ctrl_msg_request_received_with_retry_and_fail);
	RUN_TEST(test_ctrl_msg_request_received_with_retry_and_success);

	/* TODO(shurst): More PD 3.0 Tests */

	test_print_result();
}

