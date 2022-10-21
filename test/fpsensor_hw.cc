/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "compiler.h"
#include "test_util.h"
#include "fpc_private.h"
#include "board.h"

#include <sstream>
#include <ostream>
#include <iostream>

DISABLE_COMPILER_WARNING("-Wundef")
#include <gtest/gtest.h>
ENABLE_COMPILER_WARNING("-Wundef")

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

	if (IS_ENABLED(SECTION_IS_RW)) {
		std::stringstream stream;
		stream << "This is a stringstream" << std::endl;
		ccprintf("%s", stream.str().c_str());

		std::cout << "Running test" << std::endl;

		EXPECT_EQ(fpc_get_hwid(&id), EC_SUCCESS);

		/* The lower 4-bits of the sensor hardware id are a
		 * manufacturing ID that is ok to vary.
		 */
		EXPECT_EQ(fp_sensor_hwid, id >> 4);

		// Uncomment to test failure.
		// EXPECT_TRUE(false);
	};
}

// TODO: This shouldn't be necessary since tests should be registered during
// construction of static/global variables:
// https://github.com/google/googletest/blob/3026483ae575e2de942db5e760cf95e973308dd5/googletest/include/gtest/internal/gtest-internal.h#L1557-L1567
void RegisterMyTests()
{
	::testing::RegisterTest(
		"FpSensor", "CheckHardwareID", nullptr, nullptr, __FILE__,
		__LINE__, [=]() -> ::testing::Test * {
			return new FpSensor_CheckHardwareID_Test();
		});
}

extern "C" void run_test(int, const char **)
{
	// TODO: This should not be necessary:
	// https://github.com/google/googletest/blob/main/docs/advanced.md#running-a-subset-of-the-tests
	const char *_argv[] = { "ignored", "--gtest_filter=*",
				"--gtest_repeat=1" };
	int _argc = ARRAY_SIZE(_argv);

	testing::InitGoogleTest(&_argc, (char **)_argv);
	RegisterMyTests();
	int ret = RUN_ALL_TESTS();
	if (ret == 0)
		printf("Pass!\n");
	else
		printf("Fail!\n");
}
