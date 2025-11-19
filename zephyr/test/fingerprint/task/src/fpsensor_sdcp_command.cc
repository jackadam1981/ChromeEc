/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "system.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <ec_commands.h>
#include <fpsensor/fpsensor.h>
#include <fpsensor/fpsensor_state_driver.h>
#include <fpsensor/fpsensor_utils.h>
#include <fpsensor_driver.h>
#include <host_command.h>
#include <mkbp_event.h>

static int fp_sdcp_command_custom_ret;

int fp_sdcp_command(struct ec_response_fp_sdcp_claim *res)
{
	memcpy(res->pk_m, (uint8_t *)"pk_m", 4);
	return fp_sdcp_command_custom_ret;
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_ok)
{
	int ret;
	struct ec_response_fp_sdcp_claim res;

	ret = ec_cmd_fp_sdcp_claim(NULL, &res);
	zassert_equal(memcmp(res.pk_m, "pk_m", 4), 0);
	zassert_equal(EC_RES_SUCCESS, ret);
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_custom_not_ok)
{
	int ret;
	struct ec_response_fp_sdcp_claim res;

	fp_sdcp_command_custom_ret = -1;
	ret = ec_cmd_fp_sdcp_claim(NULL, &res);
	zassert_equal(memcmp(res.pk_m, "pk_m", 4), 0);
	zassert_equal(EC_RES_ERROR, ret);
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_args_not_ok)
{
	int ret;
	int to_small_res;

	ret = CROS_EC_COMMAND(NULL, EC_CMD_FP_SDCP_CLAIM, 0, NULL, 0,
			      &to_small_res, sizeof(to_small_res));
	zassert_equal(EC_RES_RESPONSE_TOO_BIG, ret);
}

ZTEST(hc_fp_sdcp, test_shell_command_sucess)
{
	char console_input[] = "fpsdcp";
	int rv = shell_execute_cmd(get_ec_shell(), console_input);
	zassert_equal(rv, EC_SUCCESS);
}

ZTEST(hc_fp_sdcp, test_shell_command_failure)
{
	char console_input[] = "fpsdcp";
	fp_sdcp_command_custom_ret = EC_RES_ERROR;
	int rv = shell_execute_cmd(get_ec_shell(), console_input);
	zassert_equal(rv, EC_RES_ERROR);
}

static void reset(void *data)
{
	ARG_UNUSED(data);

	fp_sdcp_command_custom_ret = 0;
}

ZTEST_SUITE(hc_fp_sdcp, NULL, NULL, reset, reset, NULL);
