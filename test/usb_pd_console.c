/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Test xxx.
 */

#include "common.h"
#include "math.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "usb_pe_sm.h"
#include "usb_pd.h"
#include "util.h"
#include "test_util.h"

/* Defined in implementation */
int hex8tou32(char *str, uint32_t *val);
int command_pd(int argc, char **argv);
int remote_flashing(int argc, char **argv);

static int test_port;
static int try_src;
static enum pe_dpm_request request;
static int max_volt;
static int comm_enable;
static int dev_info;
static int vdm_cmd;
static int vdm_count;
static int vdm_vid;
static uint32_t vdm_data[10];
static enum pd_dual_role_states dr_state;

/* Mock functions */
void pe_send_vdm(int port, uint32_t vid, int cmd, const uint32_t *data,
					int count)
{
	int i;

	test_port = port;
	vdm_cmd = cmd;
	vdm_count = count;
	vdm_vid = vid;

	if (data == NULL)
		for (i = 0; i < 10; i++)
			vdm_data[i] = -1;
	else
		for (i = 0; i < count; i++)
			vdm_data[i] = data[i];
}

void pe_dpm_request(int port, enum pe_dpm_request req)
{
	test_port = port;
	request = req;
}

unsigned int pd_get_max_voltage(void)
{
	return 10000;
}

void pd_request_source_voltage(int port, int mv)
{
	test_port = port;
	max_volt = mv;
}

void pd_comm_enable(int port, int enable)
{
	test_port = port;
	comm_enable = enable;
}

void tc_print_dev_info(int port)
{
	test_port = port;
	dev_info = 1;
}

void pd_set_dual_role(int port, enum pd_dual_role_states state)
{
	test_port = port;
	dr_state = state;
}

int pd_comm_is_enabled(int port)
{
	test_port = port;
	return 0;
}

int pd_get_polarity(int port)
{
	test_port = port;
	return 0;
}

uint32_t tc_get_flags(int port)
{
	test_port = port;
	return 0;
}

const char *tc_get_current_state(int port)
{
	test_port = port;
	return 0;
}

void tc_set_try_src(int en)
{
	try_src = en;
}

static int test_hex8tou32(void)
{
	char *tst_str[] = {"01234567", "89abcdef", "AABBCCDD", "EEFF0011"};
	uint32_t tst_int[] = {0x01234567, 0x89abcdef, 0xaabbccdd, 0xeeff0011};
	uint32_t val;
	int i;

	for (i = 0; i < 4; i++) {
		hex8tou32(tst_str[i], &val);
		TEST_ASSERT(val == tst_int[i]);
	}

	return EC_SUCCESS;
}

static int test_command_pd_arg_count(void)
{
	int argc;
	char *argv[] = {"pd", "", 0, 0, 0};

	for (argc = 0; argc < 3; argc++)
		TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_port_num(void)
{
	int argc = 3;
	char *argv[10] = {"pd", "5", 0, 0, 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM2);

	return EC_SUCCESS;
}

static int test_command_pd_try_src(void)
{
	int argc = 3;
	char *argv[] = {"pd", "trysrc", "1", 0, 0};

	try_src = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(try_src == 1);

	argv[2] = "0";
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(try_src == 0);

	return EC_SUCCESS;
}

static int test_command_pd_tx(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "tx", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_SNK_STARTUP);

	return EC_SUCCESS;
}

static int test_command_pd_bist_tx(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "bist_tx", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(request == DPM_REQUEST_BIST_TX);

	return EC_SUCCESS;
}

static int test_command_pd_bist_rx(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "bist_rx", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_BIST_RX);

	return EC_SUCCESS;
}

static int test_command_pd_charger(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "charger", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(request == DPM_REQUEST_SRC_STARTUP);

	return EC_SUCCESS;
}

static int test_command_pd_dev1(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dev", "20", 0};

	request = 0;
	max_volt = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_NEW_POWER_LEVEL);
	TEST_ASSERT(max_volt == 20000);

	return EC_SUCCESS;
}

static int test_command_pd_dev2(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "dev", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(request == DPM_REQUEST_NEW_POWER_LEVEL);
	TEST_ASSERT(max_volt == 10000);

	return EC_SUCCESS;
}

static int test_command_pd_disable(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "disable", 0, 0};

	comm_enable = 1;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(comm_enable == 0);

	return EC_SUCCESS;
}

static int test_command_pd_enable(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "enable", 0, 0};

	comm_enable = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(comm_enable == 1);

	return EC_SUCCESS;
}

static int test_command_pd_hard(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "hard", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_HARD_RESET_SEND);

	return EC_SUCCESS;
}

static int test_command_pd_info(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "info", 0, 0};

	dev_info = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(dev_info == 1);

	return EC_SUCCESS;
}

static int test_command_pd_soft(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "soft", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_SOFT_RESET_SEND);

	return EC_SUCCESS;
}

static int test_command_pd_swap1(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "swap", 0, 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_swap2(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "swap", "power", 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_PR_SWAP);

	return EC_SUCCESS;
}

static int test_command_pd_swap3(void)
{
	int argc = 4;
	char *argv[] = {"pd", "1", "swap", "data", 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(request == DPM_REQUEST_DR_SWAP);

	return EC_SUCCESS;
}

static int test_command_pd_swap4(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "swap", "vconn", 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_VCONN_SWAP);

	return EC_SUCCESS;
}

static int test_command_pd_swap5(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "swap", "xyz", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM3);

	return EC_SUCCESS;
}

static int test_command_pd_ping(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "ping", 0, 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(request == DPM_REQUEST_SEND_PING);

	return EC_SUCCESS;
}

static int test_command_pd_vdm1(void)
{
	int argc = 3;
	char *argv[] = {"pd", "0", "vdm", 0, 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_vdm2(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "vdm", "ping", 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_vdm3(void)
{
	int i;
	int argc = 5;
	char *argv[] = {"pd", "1", "vdm", "ping", "1"};

	vdm_cmd = 0;
	vdm_count = 0;
	for (i = 0; i < 10; i++)
		vdm_data[i] = 0;

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 1);
	TEST_ASSERT(vdm_cmd == VDO_CMD_PING_ENABLE);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count = 1);
	TEST_ASSERT(vdm_data[0] = 1);

	return EC_SUCCESS;
}

static int test_command_pd_vdm4(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "vdm", "curr", 0};

	vdm_cmd = 0;
	vdm_count = 1;
	for (i = 0; i < 10; i++)
		vdm_data[i] = 0;

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_CURRENT);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_vdm5(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "vdm", "vers", 0};

	vdm_cmd = 0;
	vdm_count = 1;
	for (i = 0; i < 10; i++)
		vdm_data[i] = 0;

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_VERSION);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_vdm6(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "vdm", "xyz", 0};

	request = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_flash1(void)
{
	int argc = 3;
	char *argv[] = {"pd", "1", "flash", 0, 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM_COUNT);

	return EC_SUCCESS;
}

static int test_command_pd_flash2(void)
{
	int argc = 3;
	char *argv[] = {"pd", "5", "flash", 0, 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_ERROR_PARAM2);

	return EC_SUCCESS;
}

static int test_command_pd_flash3(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "flash", "erase", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_FLASH_ERASE);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_flash4(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "flash", "reboot", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_REBOOT);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_flash5(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "flash", "signature", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_ERASE_SIG);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_flash6(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "flash", "info", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_READ_INFO);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_flash7(void)
{
	int i;
	int argc = 4;
	char *argv[] = {"pd", "0", "flash", "version", 0};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_VERSION);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 0);
	/* This checks that NULL was passed in for data when vdm_count is 0 */
	for (i = 0; i < 10; i++)
		TEST_ASSERT(vdm_data[i] = -1);

	return EC_SUCCESS;
}

static int test_command_pd_flash8(void)
{
	int argc = 5;
	char *argv[] = {"pd", "0", "flash", "01234567", "89abcdef"};

	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(vdm_cmd == VDO_CMD_FLASH_WRITE);
	TEST_ASSERT(vdm_vid == USB_VID_GOOGLE);
	TEST_ASSERT(vdm_count == 2);
	TEST_ASSERT(vdm_data[0] = 0x01234567);
	TEST_ASSERT(vdm_data[1] = 0x89abcdef);

	return EC_SUCCESS;
}

static int test_command_pd_dualrole1(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dualrole", "on", 0};

	dr_state = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(dr_state == PD_DRP_TOGGLE_ON);

	return EC_SUCCESS;
}

static int test_command_pd_dualrole2(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dualrole", "off", 0};

	dr_state = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(dr_state == PD_DRP_TOGGLE_OFF);

	return EC_SUCCESS;
}

static int test_command_pd_dualrole3(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dualrole", "freeze", 0};

	dr_state = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(dr_state == PD_DRP_FREEZE);

	return EC_SUCCESS;
}

static int test_command_pd_dualrole4(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dualrole", "sink", 0};

	dr_state = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(dr_state == PD_DRP_FORCE_SINK);

	return EC_SUCCESS;
}

static int test_command_pd_dualrole5(void)
{
	int argc = 4;
	char *argv[] = {"pd", "0", "dualrole", "source", 0};

	dr_state = 0;
	TEST_ASSERT(command_pd(argc, argv) == EC_SUCCESS);
	TEST_ASSERT(test_port == 0);
	TEST_ASSERT(dr_state == PD_DRP_FORCE_SOURCE);

	return EC_SUCCESS;
}


void run_test(void)
{
	test_reset();

	RUN_TEST(test_hex8tou32);
	RUN_TEST(test_command_pd_arg_count);
	RUN_TEST(test_command_pd_port_num);
	RUN_TEST(test_command_pd_try_src);
	RUN_TEST(test_command_pd_tx);
	RUN_TEST(test_command_pd_bist_tx);
	RUN_TEST(test_command_pd_bist_rx);
	RUN_TEST(test_command_pd_charger);
	RUN_TEST(test_command_pd_dev1);
	RUN_TEST(test_command_pd_dev2);
	RUN_TEST(test_command_pd_disable);
	RUN_TEST(test_command_pd_enable);
	RUN_TEST(test_command_pd_hard);
	RUN_TEST(test_command_pd_info);
	RUN_TEST(test_command_pd_soft);
	RUN_TEST(test_command_pd_swap1);
	RUN_TEST(test_command_pd_swap2);
	RUN_TEST(test_command_pd_swap3);
	RUN_TEST(test_command_pd_swap4);
	RUN_TEST(test_command_pd_swap5);
	RUN_TEST(test_command_pd_ping);
	RUN_TEST(test_command_pd_vdm1);
	RUN_TEST(test_command_pd_vdm2);
	RUN_TEST(test_command_pd_vdm3);
	RUN_TEST(test_command_pd_vdm4);
	RUN_TEST(test_command_pd_vdm5);
	RUN_TEST(test_command_pd_vdm6);
	RUN_TEST(test_command_pd_flash1);
	RUN_TEST(test_command_pd_flash2);
	RUN_TEST(test_command_pd_flash3);
	RUN_TEST(test_command_pd_flash4);
	RUN_TEST(test_command_pd_flash5);
	RUN_TEST(test_command_pd_flash6);
	RUN_TEST(test_command_pd_flash7);
	RUN_TEST(test_command_pd_flash8);
	RUN_TEST(test_command_pd_dualrole1);
	RUN_TEST(test_command_pd_dualrole2);
	RUN_TEST(test_command_pd_dualrole3);
	RUN_TEST(test_command_pd_dualrole4);
	RUN_TEST(test_command_pd_dualrole5);

	test_print_result();
}

