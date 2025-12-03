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

static int fp_sdcp_sk_f_custom_ret;

int fp_sdcp_sk_f(uint8_t *buf, size_t buf_size)
{
	static const uint8_t keyData[] = { 0xd6, 0x33, 0xa7, 0x27, 0x57, 0x64,
					   0xd2, 0xeb, 0x16, 0x07, 0x35, 0x69,
					   0xd7, 0x9e, 0xf1, 0xe6, 0x8c, 0xd4,
					   0x02, 0x54, 0x3a, 0x38, 0xce, 0x77,
					   0xf7, 0xce, 0x0d, 0x0e, 0xfc, 0xe9,
					   0xd6, 0xb0 };
	zassert_equal(sizeof(keyData), 32);
	zassert_equal(buf_size, 32);
	zassert_not_equal(buf, nullptr);
	memcpy(buf, keyData, sizeof(keyData));
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

ZTEST(hc_fp_sdcp, test_fp_sdcp_establish_ok)
{
	int ret;
	struct ec_params_fp_sdcp_establish
		params{ .pk_g = {
				0x04, 0x85, 0xAD, 0x35, 0x23, 0x05, 0x1E, 0x33,
				0x3F, 0xCA, 0xA7, 0xEA, 0xA5, 0x88, 0x33, 0x12,
				0x95, 0xA7, 0xB5, 0x98, 0x9F, 0x32, 0xEF, 0x7D,
				0xE9, 0xF8, 0x70, 0x14, 0x5E, 0x89, 0xCB, 0xDE,
				0x1F, 0xD1, 0xDC, 0x91, 0xC6, 0xE6, 0x5B, 0x1E,
				0x3C, 0x01, 0x6C, 0xE6, 0x50, 0x25, 0x5D, 0x89,
				0xCF, 0xB7, 0x8D, 0x88, 0xB9, 0x0D, 0x09, 0x41,
				0xF1, 0x09, 0x4F, 0x61, 0x55, 0x6C, 0xC4, 0x96,
				0x6B,
			} };
	ret = ec_cmd_fp_sdcp_establish(NULL, &params);
	zassert_equal(EC_RES_SUCCESS, ret);
}

static void reset(void *data)
{
	ARG_UNUSED(data);

	fp_sdcp_command_custom_ret = 0;
	fp_sdcp_sk_f_custom_ret = 0;
}

ZTEST_SUITE(hc_fp_sdcp, NULL, NULL, reset, reset, NULL);
