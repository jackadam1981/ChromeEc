/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "ec_commands.h"
#include "host_command.h"

#include <zephyr/ztest.h>

#include <algorithm>
#include <array>
#include <ascp/ascp.h>
#include <ranges>

static constexpr std::array<uint8_t, 65> pk_m = { 'p', 'k', 'm' };
static constexpr std::array<uint8_t, 64> s_goog = { 's', 'g', 'o', 'o', 'g' };
static constexpr std::array<uint8_t, 65> pk_d = { 'p', 'k', 'd' };
static constexpr std::array<uint8_t, 64> s_m = { 's', 'm' };
static constexpr std::array<uint8_t, 65> pk_f = { 'p', 'k', 'f' };
static constexpr std::array<uint8_t, 32> h_f = { 'h', 'f' };
static constexpr std::array<uint8_t, 64> s_d = { 's', 'd' };

const uint8_t *get_ascp_pk_m()
{
	return pk_m.data();
}

const uint8_t *get_ascp_s_goog()
{
	return s_goog.data();
}

const uint8_t *get_ascp_pk_d()
{
	return pk_d.data();
}

const uint8_t *get_ascp_s_m()
{
	return s_m.data();
}

const uint8_t *get_ascp_pk_f()
{
	return pk_f.data();
}

const uint8_t *get_ascp_h_f()
{
	return h_f.data();
}

const uint8_t *get_ascp_s_d()
{
	return s_d.data();
}

struct ascp_api ascp_api_instance = {
	.get_pk_m = get_ascp_pk_m,
	.get_s_goog = get_ascp_s_goog,
	.get_pk_d = get_ascp_pk_d,
	.get_s_m = get_ascp_s_m,
	.get_pk_f = get_ascp_pk_f,
	.get_h_f = get_ascp_h_f,
	.get_s_d = get_ascp_s_d,
	.get_sk_f = nullptr,
};

ZTEST(ascp_claim, test_ok)
{
	int ret;
	ec_response_fp_ascp_claim res;

	ret = ec_cmd_fp_ascp_claim(nullptr, &res);
	zassert_true(std::ranges::equal(res.pk_m, pk_m));
	zassert_true(std::ranges::equal(res.pk_d, pk_d));
	zassert_true(std::ranges::equal(res.pk_f, pk_f));
	zassert_true(std::ranges::equal(res.s_goog, s_goog));
	zassert_true(std::ranges::equal(res.s_m, s_m));
	zassert_true(std::ranges::equal(res.s_d, s_d));
	zassert_true(std::ranges::equal(res.h_f, h_f));
	zassert_equal(EC_RES_SUCCESS, ret);
}

ZTEST(ascp_claim, test_args_not_ok)
{
	int ret;
	int too_small_res;

	ret = CROS_EC_COMMAND(nullptr, EC_CMD_FP_ASCP_CLAIM, 0, nullptr, 0,
			      &too_small_res, sizeof(too_small_res));
	zassert_equal(EC_RES_RESPONSE_TOO_BIG, ret);
}

ZTEST(ascp_claim, test_shell_command_success)
{
	char console_input[] = "fpascp";
	int rv = shell_execute_cmd(get_ec_shell(), console_input);
	zassert_equal(rv, EC_SUCCESS);
}

ZTEST_SUITE(ascp_claim, nullptr, nullptr, nullptr, nullptr, nullptr);
