/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_reset_log.h"
#include "host_command.h"
#include "test/drivers/test_state.h"
#include "timer.h"

#include <zephyr/ztest.h>

extern uint16_t reboot_timeout_sec;
extern uint8_t bootstatus;

static void set_timeout(uint16_t timeout)
{
	struct ec_params_hang_detect req = {
		.command = EC_HANG_DETECT_CMD_SET_TIMEOUT,
		.reboot_timeout_sec = timeout
	};
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));
}

ZTEST_USER(ap_hang_detect, test_set_parms_good_timeout)
{
	set_timeout(EC_HANG_DETECT_MIN_TIMEOUT);
}

ZTEST_USER(ap_hang_detect, test_set_parms_bad_timeout)
{
	struct ec_params_hang_detect req = {
		.command = EC_HANG_DETECT_CMD_SET_TIMEOUT,
		.reboot_timeout_sec = EC_HANG_DETECT_MIN_TIMEOUT - 1
	};
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	zassert_equal(ec_cmd_hang_detect(&args, &req, &resp),
		      EC_RES_INVALID_PARAM);
}

ZTEST_USER(ap_hang_detect, test_reload)
{
	struct ec_params_hang_detect req;
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	set_timeout(EC_HANG_DETECT_MIN_TIMEOUT);
	req.command = EC_HANG_DETECT_CMD_RELOAD;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));

	sleep(EC_HANG_DETECT_MIN_TIMEOUT + 10);

	zassert_equal(chipset_get_shutdown_reason(), CHIPSET_RESET_HANG_REBOOT);

	req.command = EC_HANG_DETECT_CMD_GET_STATUS;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));
	zassert_equal(resp.status, EC_HANG_DETECT_AP_BOOT_EC_WDT);
	zassert_equal(bootstatus, EC_HANG_DETECT_AP_BOOT_EC_WDT);
}

ZTEST_USER(ap_hang_detect, test_cancel)
{
	struct ec_params_hang_detect req;
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;

	set_timeout(EC_HANG_DETECT_MIN_TIMEOUT);
	req.command = EC_HANG_DETECT_CMD_RELOAD;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));

	/* lets wait 1s and then cancel watchdog */
	sleep(1);
	req.command = EC_HANG_DETECT_CMD_CANCEL;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));

	/* now let's wait and check if watchdog has rebooted the AP */
	sleep(30);

	zassert_equal(bootstatus, EC_HANG_DETECT_AP_BOOT_NORMAL);
}

ZTEST_USER(ap_hang_detect, test_bootstatus)
{
	struct ec_params_hang_detect req = {
		.command = EC_HANG_DETECT_CMD_GET_STATUS,
	};
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	bootstatus = EC_HANG_DETECT_AP_BOOT_NORMAL;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));
	zassert_equal(bootstatus, EC_HANG_DETECT_AP_BOOT_NORMAL);

	bootstatus = EC_HANG_DETECT_AP_BOOT_EC_WDT;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));
	zassert_equal(bootstatus, EC_HANG_DETECT_AP_BOOT_EC_WDT);
}

ZTEST_USER(ap_hang_detect, test_clear_status)
{
	struct ec_params_hang_detect req = {
		.command = EC_HANG_DETECT_CMD_CLEAR_STATUS,
	};
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	/* simulate that watchdog kicked the board */
	bootstatus = EC_HANG_DETECT_AP_BOOT_EC_WDT;
	zassert_ok(ec_cmd_hang_detect(&args, &req, &resp));
	zassert_equal(bootstatus, EC_HANG_DETECT_AP_BOOT_NORMAL);
}

ZTEST_USER(ap_hang_detect, test_bad_command)
{
	struct ec_params_hang_detect req = {
		/* EC_HANG_DETECT_CMD_CLEAR_STATUS is the last command */
		.command = EC_HANG_DETECT_CMD_CLEAR_STATUS + 1,
	};
	struct ec_response_hang_detect resp;
	struct host_cmd_handler_args args;

	zassert_equal(ec_cmd_hang_detect(&args, &req, &resp),
		      EC_RES_INVALID_PARAM);
}

ZTEST_SUITE(ap_hang_detect, drivers_predicate_post_main, NULL, NULL, NULL,
	    NULL);
