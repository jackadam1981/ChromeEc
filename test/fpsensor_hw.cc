/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "test_util.h"
#include "fpc_private.h"
#include "board.h"

#include <gtest/gtest.h>

#include <sstream>
#include <ostream>
#include <iostream>

#ifdef SECTION_IS_RW
#include "fpc/fpc_sensor.h"
static const uint32_t fp_sensor_hwid = FP_SENSOR_HWID;
#else
static const uint32_t fp_sensor_hwid = UINT32_MAX;
#endif

/* Hardware-dependent smoke test that makes a SPI transaction with the
 * fingerprint sensor.
 */

TEST(FpSensor, CheckHardwareID)
{
	uint16_t id = 0;

	ccprintf("Test starting\n");

	if (IS_ENABLED(SECTION_IS_RW)) {
		ccprintf("Getting HWID\n");
		EXPECT_EQ(fpc_get_hwid(&id), EC_SUCCESS);

		ccprintf("Checking HWID\n");
		/* The lower 4-bits of the sensor hardware id are a
		 * manufacturing ID that is ok to vary.
		 */
		EXPECT_EQ(fp_sensor_hwid, id >> 4);

		ccprintf("Expect failure\n");
		EXPECT_TRUE(false);
	};
}

extern "C" void run_test(int argc, const char **argv)
{
	ccprintf("Running run_test() from %s\n", __FILE__);
	testing::InitGoogleTest(&argc, (char **)argv);
	int ret = RUN_ALL_TESTS();
	ccprintf("Return: %d\n", ret);
}
