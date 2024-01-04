/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"

#include <string.h>

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#ifdef TODO_HOST_COMMANDS
ZTEST(pdc_host_commands, test_ec_cmd_pd_chip_info__v0)
{
	struct ec_params_pd_chip_info req = {
		.port = 0,
		.live = 0,
	};
	struct ec_response_pd_chip_info resp = { 0 };

	zassert_ok(ec_cmd_pd_chip_info(NULL, &req, &resp));

	zassert_equal(resp.vendor_id, 0xABCD, "Actual VID: $%04x",
		      resp.vendor_id);
}

ZTEST_SUITE(pdc_host_commands, NULL, NULL, NULL, NULL, NULL);
#endif /* TODO_HOST_COMMANDS */
