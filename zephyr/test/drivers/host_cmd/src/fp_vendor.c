/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "host_command.h"
#include "test/drivers/test_state.h"
#include "test/drivers/utils.h"

#include <zephyr/drivers/emul.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

FAKE_VALUE_FUNC(int, fp_vendor_commad, uint32_t, uint8_t *, size_t);

const static uint32_t fp_vendor_param1 = 0xaaaa;

int fp_vendor_commad_custom(uint32_t param, uint8_t *buf, size_t buf_size)
{
	zassert_equal(param, fp_vendor_param1);

	return 0;
}

ZTEST(hc_fp_vendor, test_fp_vendor_ok)
{
	struct ec_params_fp_vendor params = {
		.param1 = fp_vendor_param1,
	};

	fp_vendor_commad_fake.custom_fake = fp_vendor_commad_custom;
	ec_cmd_fp_vendor(NULL, &params);
}

static void reset(void *data)
{
	ARG_UNUSED(data);

	RESET_FAKE(fp_vendor_commad);
}

ZTEST_SUITE(hc_fp_vendor, drivers_predicate_post_main, NULL, reset, reset,
	    NULL);
