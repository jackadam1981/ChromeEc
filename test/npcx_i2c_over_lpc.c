/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test the state machine of the i2c over lpc nuvuton driver.
 * Protocol is defined here:
 * https://drive.google.com/file/d/0B0DO3Pn_jl5cc2xvbkZaVkpjTDNURmwwZV9XU1BxaVVTdDFB/view
 *
 */
#include "hooks.h"
#include "i2c_over_lpc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

uint8_t msg_from_host_region[LPC_HOST_MEM_WINDW_SIZE];


/* Tests */
#define PATTERN "abcdefghij"
#define PATTERN_LEN (sizeof(PATTERN))
#define NERTTAP "jihgfedcba"

int interaction_idx;
uint8_t ec_set_unset[][2] = {
	{ NPCX_IOL_CORE_CONT, 0},
	{ 0, NPCX_IOL_CORE_CONT},
	{ NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, 0},
	{ 0, NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND},
};
uint8_t host_set_unset[][2] = {
	{ NPCX_IOL_HOST_ACK, NPCX_IOL_HOST_REQUEST},
	{ 0, NPCX_IOL_HOST_ACK},
	{ NPCX_IOL_HOST_ACK, 0},
	{ 0, NPCX_IOL_HOST_ACK},
};

static int test_send_single_small_command(void)
{
	msg_from_host->host_request.writes_nb = PATTERN_LEN;
	msg_from_host->host_request.reads_nb = PATTERN_LEN;
	memcpy(msg_from_host->host_request.write_buf, PATTERN, PATTERN_LEN);
	msg_from_host->semaphore = NPCX_IOL_HOST_REQUEST;
	/* A write trigger an interrupt */
	task_set_event(TASK_ID_IOLCMD, TASK_EVENT_IOL_PENDING, 0);
	while (interaction_idx < ARRAY_SIZE(ec_set_unset))
		msleep(100);
	TEST_ASSERT(!memcmp(msg_from_host->core_request.read_buf, NERTTAP,
				PATTERN_LEN));
	return EC_SUCCESS;
}

int test_send_single_small_command_check(int len, uint8_t *buffer)
{
	TEST_ASSERT(len == PATTERN_LEN);
	TEST_ASSERT(!memcmp(buffer, PATTERN, PATTERN_LEN));
	return 0;
}

int npcx_iol_host_int(void)
{
	TEST_ASSERT(interaction_idx < ARRAY_SIZE(ec_set_unset));
	if (ec_set_unset[interaction_idx][0])
		TEST_ASSERT(msg_from_host->semaphore &
				ec_set_unset[interaction_idx][0]);
	TEST_ASSERT(!(msg_from_host->semaphore &
			ec_set_unset[interaction_idx][1]));
	msg_from_host->semaphore = (msg_from_host->semaphore &
			~host_set_unset[interaction_idx][1]) |
		host_set_unset[interaction_idx][0];
	interaction_idx++;
	task_set_event(TASK_ID_IOLCMD, TASK_EVENT_IOL_PENDING, 0);
	return 0;
}


void i2c_hid_process(int read, int len, uint8_t *buffer,
		void (*send_response)(int len))
{
	test_send_single_small_command_check(len, buffer);
	memcpy(buffer, NERTTAP, PATTERN_LEN);
	send_response(PATTERN_LEN);
}

void init_hook(void)
{
	msg_from_host = (struct npcx_iol_msg *)msg_from_host_region;
}
DECLARE_HOOK(HOOK_INIT, init_hook, HOOK_PRIO_DEFAULT);

void run_test(void)
{
	test_reset();
	wait_for_task_started();

	RUN_TEST(test_send_single_small_command);

	test_print_result();
}
