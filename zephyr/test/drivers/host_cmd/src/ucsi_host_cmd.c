/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ec_commands.h"
#include "host_command.h"
#include "test/drivers/test_state.h"

#include <zephyr/fff.h>
#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include <usbc/ppm.h>

struct ucsi_ppm_device {
	void *ptr;
};

extern struct ucsi_ppm_device *ppm_dev;

FAKE_VALUE_FUNC(int, ucsi_ppm_write, struct ucsi_ppm_device *, unsigned int,
		const void *, size_t);

FAKE_VALUE_FUNC(int, ucsi_ppm_read, struct ucsi_ppm_device *, unsigned int,
		void *, size_t);

FAKE_VALUE_FUNC(int, ucsi_ppm_register_notify, struct ucsi_ppm_device *,
		ucsi_ppm_notify_cb *, void *);

ZTEST_USER(ucsi_host_cmd, test_hc_ppm_get_success)
{
	struct ec_params_ucsi_ppm_get params = {
		.offset = 1,
		.size = 1,
	};
	enum ec_status rv;
	struct ucsi_ppm_device fake_ppm_device;

	ppm_dev = &fake_ppm_device;
	ucsi_ppm_write_fake.return_val = 0;
	rv = ec_cmd_ucsi_ppm_get(NULL, &params);
	zassert_equal(rv, EC_RES_SUCCESS);
	zassert_equal(ucsi_ppm_read_fake.call_count, 1);
	zassert_equal(ucsi_ppm_read_fake.arg0_val, ppm_dev);
	zassert_equal(ucsi_ppm_read_fake.arg1_val, 1);
}

ZTEST_USER(ucsi_host_cmd, test_hc_ppm_get)
{
	struct ec_params_ucsi_ppm_get params = {
		.offset = 1,
		.size = 1,
	};
	enum ec_status rv;

	ppm_dev = NULL;
	rv = ec_cmd_ucsi_ppm_get(NULL, &params);
	zassert_equal(rv, EC_RES_UNAVAILABLE);
	zassert_equal(ucsi_ppm_read_fake.call_count, 0);
}

ZTEST_USER(ucsi_host_cmd, test_hc_ppm_set_success)
{
	struct ec_params_ucsi_ppm_set params = {
		.offset = 1,
		.data = {},
	};
	enum ec_status rv;
	struct ucsi_ppm_device fake_ppm_device;

	ppm_dev = &fake_ppm_device;
	ucsi_ppm_write_fake.return_val = 0;
	rv = ec_cmd_ucsi_ppm_set(NULL, &params);
	zassert_equal(rv, EC_RES_SUCCESS);
	zassert_equal(ucsi_ppm_write_fake.call_count, 1);
	zassert_equal(ucsi_ppm_write_fake.arg0_val, ppm_dev);
	zassert_equal(ucsi_ppm_write_fake.arg1_val, 1);
	zassert_equal(ucsi_ppm_write_fake.arg2_val, params.data);
}

ZTEST_USER(ucsi_host_cmd, test_hc_ppm_set)
{
	struct ec_params_ucsi_ppm_set params = {
		.offset = 0,
		.data = {},
	};
	enum ec_status rv;

	ppm_dev = NULL;
	rv = ec_cmd_ucsi_ppm_set(NULL, &params);
	zassert_equal(rv, EC_RES_UNAVAILABLE);
	zassert_equal(ucsi_ppm_write_fake.call_count, 0);
}

ZTEST_SUITE(ucsi_host_cmd, drivers_predicate_post_main, NULL, NULL, NULL, NULL);
