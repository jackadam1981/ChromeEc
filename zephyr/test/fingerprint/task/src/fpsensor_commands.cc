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

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, mkbp_send_event, uint8_t);

const static uint32_t fp_vendor_param1 = 0xabcd;
static int fp_vendor_commad_custom_ret;
static int is_locked;

int system_is_locked(void)
{
	return is_locked;
}

int fp_vendor_commad(uint32_t param, uint8_t *buf, size_t buf_size)
{
	zassert_equal(param, fp_vendor_param1);

	return fp_vendor_commad_custom_ret;
}

ZTEST(hc_fp_vendor, test_fp_vendor_ok)
{
	struct ec_params_fp_vendor params = {
		.param1 = fp_vendor_param1,
	};
	int ret;

	ret = ec_cmd_fp_vendor(NULL, &params);
	zassert_equal(EC_RES_SUCCESS, ret);
}

ZTEST(hc_fp_vendor, test_fp_vendor_custom_not_ok)
{
	struct ec_params_fp_vendor params = {
		.param1 = fp_vendor_param1,
	};
	int ret;

	fp_vendor_commad_custom_ret = -1;
	ret = ec_cmd_fp_vendor(NULL, &params);
	zassert_equal(EC_RES_ERROR, ret);
}

ZTEST(hc_fp_vendor, test_fp_vendor_locked)
{
	struct ec_params_fp_vendor params = {
		.param1 = fp_vendor_param1,
	};
	int ret;

	is_locked = 1;
	ret = ec_cmd_fp_vendor(NULL, &params);
	zassert_equal(EC_RES_ACCESS_DENIED, ret);
}

static void reset(void *data)
{
	ARG_UNUSED(data);

	is_locked = 0;
	fp_vendor_commad_custom_ret = 0;
}

ZTEST_SUITE(hc_fp_vendor, NULL, NULL, reset, reset, NULL);
