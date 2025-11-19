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

#include <algorithm>
#include <ec_commands.h>
#include <fpsensor/fpsensor.h>
#include <fpsensor/fpsensor_state_driver.h>
#include <fpsensor/fpsensor_utils.h>
#include <fpsensor_driver.h>
#include <host_command.h>
#include <mkbp_event.h>
#include <ranges>

static constexpr std::array<uint8_t, 65> pk_f = {
	0x04, 0x48, 0x04, 0x5c, 0x21, 0x5b, 0x8d, 0x9b, 0x23, 0x31, 0x95,
	0x0e, 0x2a, 0x8d, 0x8f, 0x26, 0x22, 0x66, 0xe9, 0x59, 0xc2, 0x37,
	0xc9, 0x4f, 0x94, 0x73, 0x27, 0xc2, 0xb1, 0x11, 0xa0, 0x01, 0x78,
	0x13, 0x2f, 0x08, 0x9e, 0x94, 0xe7, 0x6d, 0x58, 0x76, 0xa1, 0x1a,
	0x20, 0xda, 0x0c, 0xc1, 0xef, 0x91, 0xaf, 0xb1, 0x5d, 0x48, 0x81,
	0x1b, 0x6b, 0xe1, 0x6e, 0xc0, 0xd5, 0xfb, 0x9d, 0x69, 0x82
};

static int fp_sdcp_command_custom_ret;

int fp_sdcp_command(ec_response_fp_sdcp_claim *res)
{
	std::ranges::copy(pk_f, std::begin(res->pk_f));
	return fp_sdcp_command_custom_ret;
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_ok)
{
	int ret;
	ec_response_fp_sdcp_claim res;

	ret = ec_cmd_fp_sdcp_claim(nullptr, &res);
	zassert_true(std::ranges::equal(res.pk_f, pk_f));
	zassert_equal(EC_RES_SUCCESS, ret);
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_custom_not_ok)
{
	int ret;
	ec_response_fp_sdcp_claim res;

	fp_sdcp_command_custom_ret = -1;
	ret = ec_cmd_fp_sdcp_claim(nullptr, &res);
	zassert_true(std::ranges::equal(res.pk_f, pk_f));
	zassert_equal(EC_RES_ERROR, ret);
}

ZTEST(hc_fp_sdcp, test_fp_sdcp_args_not_ok)
{
	int ret;
	int to_small_res;

	ret = CROS_EC_COMMAND(nullptr, EC_CMD_FP_SDCP_CLAIM, 0, nullptr, 0,
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

ZTEST_SUITE(hc_fp_sdcp, nullptr, nullptr, reset, reset, nullptr);
