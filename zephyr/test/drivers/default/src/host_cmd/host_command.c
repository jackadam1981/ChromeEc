#include <zephyr/ztest.h>
#include "include/lpc.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/shell/shell.h>
#include <zephyr/ztest.h>

#include "console.h"
#include "ec_commands.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

/* EC_CMD_GET_FEATURES */
ZTEST_USER(host_cmd_host_command, test_host_command_ec_cmd_get_features)
{
	struct ec_response_get_features response;
	struct host_cmd_handler_args args =
		BUILD_HOST_COMMAND_RESPONSE(EC_CMD_GET_FEATURES, 0, response);

	int rv = host_command_process(&args);

	zassert_equal(rv, EC_RES_SUCCESS, "Expected %d, but got %d",
		      EC_RES_SUCCESS, rv);

	/* Check features returned */
	uint32_t feature_mask;

	feature_mask = EC_FEATURE_MASK_0(EC_FEATURE_FLASH);
	feature_mask |= EC_FEATURE_MASK_0(EC_FEATURE_MOTION_SENSE);
	feature_mask |= EC_FEATURE_MASK_0(EC_FEATURE_KEYB);
	zassert_true((response.flags[0] & feature_mask),
		     "Known features were not returned.");
	feature_mask = EC_FEATURE_MASK_1(EC_FEATURE_UNIFIED_WAKE_MASKS);
	feature_mask |= EC_FEATURE_MASK_1(EC_FEATURE_HOST_EVENT64);
	feature_mask |= EC_FEATURE_MASK_1(EC_FEATURE_EXEC_IN_RAM);
	zassert_true((response.flags[1] & feature_mask),
		     "Known features were not returned.");
}

ZTEST_SUITE(host_cmd_host_command, NULL, NULL, NULL, NULL, NULL);
