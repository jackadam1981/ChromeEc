/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "config.h"
#include "fpsensor.h"
#include "test_util.h"

static const uint32_t fp_sensor_hwid = FP_SENSOR_HWID_FPC;

#if defined(CONFIG_FP_SENSOR_ELAN80) || defined(CONFIG_FP_SENSOR_ELAN515)
static const uint32_t fp_sensor_hwid = 0x0903; // from PID macro in
					       // elan_setting.h

int generic_get_hwid(uint16_t *id)
{
	struct ec_response_fp_info resp;
	int rc = fp_driver->sensor_get_info(&resp);
	*id = (uint16_t)resp.product_id;
	return rc;
}

#else
static const uint32_t fp_sensor_hwid = FP_SENSOR_HWID_FPC;
int generic_get_hwid(uint16_t *id)
{
	uint16_t tmpid = 0;
	int rc = fpc_get_hwid(&tmpid);
	*id = tmpid >> 4;
	return rc;
}
#endif

/* Hardware-dependent smoke test that makes a SPI transaction with the
 * fingerprint sensor.
 */
test_static int test_fp_check_hwid(void)
{
	uint16_t id = 0;

	TEST_EQ(generic_get_hwid(&id), EC_SUCCESS, "%d");
	/* The lower 4-bits of the sensor hardware id are a
	 * manufacturing ID that is ok to vary.
	 */
	TEST_EQ(fp_sensor_hwid, id, "%d");
	return EC_SUCCESS;
}

extern "C" void run_test(int argc, const char **argv)
{
	RUN_TEST(test_fp_check_hwid);
	test_print_result();
}
