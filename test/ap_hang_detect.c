/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "task.h"
#include "test_util.h"
#include "timer.h"
#include "util.h"

#include <stdbool.h>

struct ec_hang_detect_params hdparams;
uint8_t bootstatus;

/* Tests */
test_static enum ec_error_list test_ap_hang_detect_set_parms_good_timeout(void)
{
	struct ec_hang_detect_req req = {};

	req.command = EC_HANG_DETECT_CMD_SET_PARAMS;
	req.params.args = 0;
	req.params.reboot_timeout_sec = EC_HANG_DETECT_MIN_TIMEOUT;

	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_SUCCESS, "%d");
	TEST_EQ(memcmp(&(req.params), &hdparams, sizeof(hdparams)), 0, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ap_hang_detect_set_parms_bad_timeout(void)
{
	struct ec_hang_detect_req req = {};

	req.command = EC_HANG_DETECT_CMD_SET_PARAMS;
	req.params.args = 0;
	req.params.reboot_timeout_sec = EC_HANG_DETECT_MIN_TIMEOUT - 1;

	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_INVALID_PARAM, "%d");
	TEST_NE(memcmp(&(req.params), &hdparams, sizeof(hdparams)), 0, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ap_hang_detect_reload(void)
{
	struct ec_hang_detect_req req = {};

	req.command = EC_HANG_DETECT_CMD_RELOAD;

	TEST_EQ(test_ap_hang_detect_set_parms_good_timeout(), EC_SUCCESS, "%d");
	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_SUCCESS, "%d");

	/* lets wait 5s (reboot timeout set by
	 * test_ap_hang_detect_set_parms_good() + 10s
	 */
	sleep(EC_HANG_DETECT_MIN_TIMEOUT + 10);

	TEST_EQ(chipset_get_shutdown_reason(), CHIPSET_RESET_HANG_REBOOT, "%d");
	TEST_EQ(bootstatus, EC_HANG_DETECT_AP_BOOT_EC_WDT, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ap_hang_detect_cancel(void)
{
	struct ec_hang_detect_req req = {};

	bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;
	req.command = EC_HANG_DETECT_CMD_RELOAD;

	TEST_EQ(test_ap_hang_detect_set_parms_good_timeout(), EC_SUCCESS, "%d");
	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_SUCCESS, "%d");
	/* lets wait 1s and then cancel watchdog */
	sleep(1);
	req.command = EC_HANG_DETECT_CMD_CANCEL;
	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_SUCCESS, "%d");
	/* now let's wait and check if watchdog has rebooted the AP */
	sleep(30);
	TEST_EQ(bootstatus, EC_HANG_DETECT_AP_BOOT_NORMAL, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ap_hang_detect_get_status(void)
{
	struct ec_hang_detect_req req = {};
	struct ec_hang_detect_resp resp = {};

	req.command = EC_HANG_DETECT_CMD_GET_STATUS;
	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       &resp, sizeof(resp)),
		EC_RES_SUCCESS, "%d");
	TEST_EQ(resp.status, bootstatus, "%d");

	return EC_SUCCESS;
}

test_static enum ec_error_list test_ap_hang_detect_unknown_cmd(void)
{
	struct ec_hang_detect_req req = {};

	req.command = EC_HANG_DETECT_CMD_CLEAR_STATUS + 1;
	TEST_EQ(test_send_host_command(EC_CMD_HANG_DETECT, 0, &req, sizeof(req),
				       NULL, 0),
		EC_RES_INVALID_PARAM, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();

	RUN_TEST(test_ap_hang_detect_set_parms_good_timeout);
	RUN_TEST(test_ap_hang_detect_set_parms_bad_timeout);
	RUN_TEST(test_ap_hang_detect_reload);
	RUN_TEST(test_ap_hang_detect_cancel);
	RUN_TEST(test_ap_hang_detect_get_status);
	RUN_TEST(test_ap_hang_detect_unknown_cmd);

	test_print_result();
}
