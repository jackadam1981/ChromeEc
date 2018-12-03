/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test the state machine of the i2c over lpc nuvuton driver.
 * Protocol is defined here:
 * https://goto.google.com/nuvoton-ec-i2c
 *
 */
#include "ec_commands.h"
#include "hooks.h"
#include "i2c_over_lpc.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

uint8_t msg_from_host_region[EC_HOST_CMD_REGION_SIZE];

#define PATTERN 'a'
int inter_idx, inter_max;
struct inter {
	uint8_t exp_core_sem;
	uint8_t set;
	uint8_t unset;
	int (*buffer_operation)(int arg);
	int arg;
};

struct inter *inter_arr;

/*
 * Test interrupt handler: It simulates the host by setting the host side of the
 * semaphore field.
 * We also use that time to check the EC is setting the memory region properly.
 */
int npcx_iol_host_int(int semaphore)
{
	cprints(CC_I2C, "step %d", inter_idx);
	TEST_ASSERT(inter_idx < inter_max);
	TEST_ASSERT((semaphore & NPCX_IOL_CORE_SEM_MASK) ==
		    inter_arr[inter_idx].exp_core_sem);
	semaphore = (semaphore &
			~inter_arr[inter_idx].unset) |
		inter_arr[inter_idx].set;
	if (inter_arr[inter_idx].buffer_operation)
		TEST_ASSERT(inter_arr[inter_idx].buffer_operation(
				inter_arr[inter_idx].arg) == 0);
	inter_idx++;
	NPCX_IOL_SEM = semaphore;
	task_set_event(TASK_ID_IOLCMD, TASK_EVENT_IOL_PENDING, 0);
	return 0;
}

/* Functions needed to check patterns. */
int setup_cont_pattern(int len)
{
	memset(msg_from_host->host_request_cont.write_buf, PATTERN,
	       MIN(len, MSG_FROM_HOST_SIZE));
	return 0;
}

int check_first_pattern(int len)
{
	int i;

	TEST_ASSERT(msg_from_host->core_request.status == 1);
	for (i = 0; i < len; i++)
		TEST_ASSERT(msg_from_host->core_request.read_buf[i] ==
			    PATTERN + 1);
	return 0;
}

int check_cont_pattern(int len)
{
	int i;

	for (i = 0; i < len; i++)
		TEST_ASSERT(msg_from_host->core_request_cont.read_buf[i] ==
			    PATTERN + 1);
	return 0;
}


/* Simulate a hid command that process the i2c packet. */
int test_send_command_check(int len, uint8_t *buffer)
{
	int i;

	for (i = 0; i < len; i++)
		if (buffer[i] != PATTERN)
			break;
	TEST_ASSERT(i == len);
	return 0;
}

void i2c_hid_process(int len, uint8_t *buffer,
		void (*send_response)(int len))
{
	test_send_command_check(len, buffer);
	memset(buffer, PATTERN + 1, len);
	send_response(len);
}

/* small test: send and receive a 10 bytes i2c packets. */
#define SMALL_PATTERN_LEN 10

int setup_small_pattern(int len)
{
	msg_from_host->host_request.writes_nb = len;
	msg_from_host->host_request.reads_nb = len;
	memset(msg_from_host->host_request.write_buf, PATTERN, len);
	return 0;
}

struct inter host_cmd_transition[] = {
	{ 0, NPCX_IOL_HOST_REQUEST, 0, setup_small_pattern, SMALL_PATTERN_LEN},
	{ NPCX_IOL_CORE_CONT, NPCX_IOL_HOST_ACK, NPCX_IOL_HOST_REQUEST, NULL,
		0},
	{ 0, 0, NPCX_IOL_HOST_ACK, NULL, 0},
	{ NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, NPCX_IOL_HOST_ACK, 0,
		check_first_pattern, SMALL_PATTERN_LEN },
	{ NPCX_IOL_CORE_SEND, 0, NPCX_IOL_HOST_ACK, NULL, 0 },
	{ 0, 0, NPCX_IOL_HOST_ACK, NULL, 0 }
};

static int test_send_small_command(void)
{
	inter_arr = host_cmd_transition;
	inter_idx = 0;
	inter_max = ARRAY_SIZE(host_cmd_transition);
	/* trigger an interrupt to start the process. */
	npcx_iol_host_int(NPCX_IOL_HOST_REQUEST);
	while (inter_idx < inter_max)
		msleep(100);
	return EC_SUCCESS;
}

/* large test: send a 64 bytes, and receive a 32 bytes i2c packets. */
int setup_large_pattern(int len)
{
	msg_from_host->host_request.writes_nb = len;
	msg_from_host->host_request.reads_nb = len / 2;
	memset(msg_from_host->host_request.write_buf, PATTERN,
	       MIN(len, MSG_FROM_HOST_SIZE - sizeof(msg_from_host->host_request)));
	return 0;
}

struct inter host_multiple_cmd_transition[] = {
	{ 0, NPCX_IOL_HOST_REQUEST, 0, setup_large_pattern, 2 *
		EC_HOST_CMD_REGION_SIZE },
	{ NPCX_IOL_CORE_CONT, NPCX_IOL_HOST_ACK, NPCX_IOL_HOST_REQUEST, NULL,
		0 },
	{ 0, 0, NPCX_IOL_HOST_ACK, setup_cont_pattern, 134 },
	{ NPCX_IOL_CORE_CONT, NPCX_IOL_HOST_ACK, 0, NULL, 0 },
	{ 0, 0, NPCX_IOL_HOST_ACK, setup_cont_pattern, 7 },
	{ NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, NPCX_IOL_HOST_ACK, 0,
		check_first_pattern, 30 },
	{ NPCX_IOL_CORE_SEND, 0, NPCX_IOL_HOST_ACK, NULL, 0},
	{ NPCX_IOL_CORE_CONT | NPCX_IOL_CORE_SEND, NPCX_IOL_HOST_ACK, 0,
		check_cont_pattern, 2 },
	{ NPCX_IOL_CORE_SEND, 0, NPCX_IOL_HOST_ACK, NULL, 0 },
	{ 0, 0, NPCX_IOL_HOST_ACK, NULL, 0},
};

static int test_send_large_command(void)
{
	inter_arr = host_multiple_cmd_transition;
	inter_idx = 0;
	inter_max = ARRAY_SIZE(host_multiple_cmd_transition);
	/* trigger an interrupt to start the process. */
	npcx_iol_host_int(NPCX_IOL_HOST_REQUEST);
	while (inter_idx < inter_max)
		msleep(100);
	return EC_SUCCESS;
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

	RUN_TEST(test_send_small_command);
	RUN_TEST(test_send_large_command);

	test_print_result();
}
