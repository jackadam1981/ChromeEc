/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "test_util.h"
#include "fpc_private.h"
//#include "fpc/fpc_sensor.h"

#include "bep/fpc1025_private.h"


//#if defined(CONFIG_FP_SENSOR_FPC1025)
//#include "bep/fpc1025_private.h"
//#elif defined(CONFIG_FP_SENSOR_FPC1035)
//#include "bep/fpc1035_private.h"
//#elif defined(CONFIG_FP_SENSOR_FPC1145)
//#include "libfp/fpc1145_private.h"
//#else
//#error "Sensor type not defined!"
//#endif

/* Hardware-dependent smoke test that makes a SPI transaction with the
 * fingerprint sensor.
 */
test_static int test_fp_check_hwid(void)
{
	if (IS_ENABLED(SECTION_IS_RW))
		TEST_EQ(fpc_check_hwid(), FP_SENSOR_HWID, "%d");

	return EC_SUCCESS;
}


void run_test(int argc, char **argv)
{
	RUN_TEST(test_fp_check_hwid);
	test_print_result();
}
