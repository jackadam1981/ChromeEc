/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mock/rollback_latest_mock.h"
#include "rollback.h"
#include "rollback_private.h"
#include "string.h"
#include "system.h"
#include "test_util.h"

extern int get_latest_rollback(struct rollback_data *data);

test_static int test_trial(void)
{
	int rv = 1;

	ccprints("The value of rv is: %d", rv);

	return EC_RES_SUCCESS;
}

test_static int test_trial_2(void)
{
	struct rollback_data test_data;

	mock_ctrl_latest_rollback.output_type = GET_LATEST_ROLLBACK_REAL;

	TEST_ASSERT(get_latest_rollback(&test_data) == 0);

	ccprints("The value of cookie is : %d", test_data.cookie);
	ccprints("The value of id is : %d", test_data.id);
	ccprints("The value of version is : %d",
		 test_data.rollback_min_version);

	for (int i = 0; i < 32; ++i) {
		ccprints("The value of test_data.secret[%d] is: 0x%02x.", i,
			 test_data.secret[i]);
	}

	ccprints("The value of EC_ERROR_UNKNOWN is: %d", EC_ERROR_UNKNOWN);

	// It can read things from include/rollback.h
	ccprints("The value of FIRAS is: %d", FIRAS);

	return EC_SUCCESS;
}

test_static int test_get_rollback_secret_latest_rollback_fail(void)
{
	struct rollback_data test_data;
	// uint8_t secret[32] = { 0 };
	int rv;

	rv = MyFunction2(3);
	ccprints("The value of rv is: %d", rv);

	// it will generate an error for MyFunction3 undefined.
	// rv = MyFunction3(3);
	// ccprints("The value of rv is: %d", rv);
	ccprints("The value of FIRAS2 is: %d", FIRAS2);

	mock_ctrl_latest_rollback.output_type = GET_LATEST_ROLLBACK_FAIL;

	TEST_ASSERT(get_latest_rollback(&test_data) == -5);

	// No function fined inside common/rollback.c is visible.
	// rv = rollback_get_secret(secret);

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	RUN_TEST(test_trial);
	RUN_TEST(test_trial_2);
	RUN_TEST(test_get_rollback_secret_latest_rollback_fail);
	test_print_result();
}