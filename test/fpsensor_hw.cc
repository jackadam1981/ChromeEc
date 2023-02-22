/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "board.h"
#include "common.h"
#include "fpsensor.h"
#include "test_util.h"

/* Hardware-dependent smoke test that makes a SPI transaction with the
 * fingerprint sensor.
 */
test_static int test_fp_check_hwid(void)
{
	uint16_t id = 0;
	int rc;
	const uint32_t fp_sensor_hwid = fp_driver->sensor_hwid;
	if (IS_ENABLED(SECTION_IS_RW)) {
		struct ec_response_fp_info info;
		rc = fp_driver->sensor_get_info(&info);
		TEST_EQ(rc, EC_SUCCESS, "%d");
		TEST_EQ(info.model_id, 0x123, "%d");
		// TEST_EQ(info.errors, 0, "%d");
		/* The lower 4-bits of the sensor hardware id are a
		 * manufacturing ID that is ok to vary.
		 */
		TEST_EQ(fp_sensor_hwid, id >> 4, "%d");
	};
	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_check_hwid);
	test_print_result();
}
