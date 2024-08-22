/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "fpc_private.h"
#include "test_util.h"

#ifdef SECTION_IS_RW
#include "fpsensor_driver.h"
static const uint32_t fp_sensor_hwid = FP_SENSOR_HWID;
#else
static const uint32_t fp_sensor_hwid = UINT32_MAX;
#endif

// #ifdef SECTION_IS_RW
// #include "fpsensor_driver.h"
// #endif

/* Hardware-dependent smoke test that makes a SPI transaction with the
 * fingerprint sensor.
 */
test_static int test_fp_check_hwid(void)
{
	uint16_t id = 0;

	if (IS_ENABLED(CONFIG_FP_SENSOR_FPC1025) || IS_ENABLED(CONFIG_FP_SENSOR_FPC1145)) {
		TEST_EQ(fpc_get_hwid(&id), EC_SUCCESS, "%d");
	} else if (IS_ENABLED(CONFIG_FP_SENSOR_ELAN80SG)) {
		TEST_EQ(elan_get_hwid(&id), EC_SUCCESS, "%d");
	}

	/* The lower 4-bits of the sensor hardware id are a
	 * manufacturing ID that is ok to vary.
	 */
	TEST_EQ(fp_sensor_hwid, id >> 4, "%d");

	return EC_SUCCESS;
}

void run_test(int argc, const char **argv)
{
	test_reset();
	if (IS_ENABLED(SECTION_IS_RW)) {
		RUN_TEST(test_fp_check_hwid);
	}
	test_print_result();
}
