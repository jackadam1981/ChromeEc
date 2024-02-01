/* Copyright 2024 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "console.h"
#include "hooks.h"
#include "motion_sense.h"
#include "motion_sense_fifo.h"
#include "pw_function/function.h"
#include "pw_string/string_builder.h"
#include "task.h"
#include "test_lib/shell/shell.h"

#include <zephyr/shell/shell_dummy.h>

#include <gtest/gtest.h>
#include <iostream>

class MotionSenseInterruptToggle : public ::testing::Test {
    protected:
	void SetUp() override
	{
		TearDown();
		hook_notify(HOOK_CHIPSET_RESUME);
		k_msleep(500);
	}
	void TearDown() override
	{
		for (auto *s = begin; s < end; ++s) {
			for (int cfg = (int)SENSOR_CONFIG_AP;
			     cfg < (int)SENSOR_CONFIG_MAX; ++cfg) {
				s->config[cfg].odr = 0;
				s->config[cfg].ec_rate = 0;
			}
		}
		motion_sense_fifo_reset();
	}

	void VerifyHasData(
		pw::Function<bool(const struct ec_response_motion_sensor_data &)>
			predicate)
	{
		struct ec_response_motion_sensor_data data;
		uint16_t out_size;
		bool success = false;

		while (motion_sense_fifo_read(sizeof(data), 1, &data,
					      &out_size) > 0) {
			if (predicate(data)) {
				success = true;
			}
		}
		if (!success) {
			GTEST_FAIL();
		}
	}

	void VerifyEachData(
		pw::Function<bool(const struct ec_response_motion_sensor_data &)>
		        predicate
		) {
		struct ec_response_motion_sensor_data data;
		uint16_t out_size;
		bool success = true;

		while (motion_sense_fifo_read(sizeof(data), 1, &data,
					      &out_size) > 0) {
			if (!predicate(data)) {
				success = false;
			}
		}
		if (!success) {
			GTEST_FAIL();
		}
	}

	motion_sensor_t *begin = &motion_sensors[0];
	motion_sensor_t *end = &motion_sensors[motion_sensor_count];
};

#if CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS > 0
#define LOWER_THRESHOLD_MHZ (900000 / CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS)
#define UPPER_THRESHOLD_MHZ \
	(1100000 / CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS)
BUILD_ASSERT(1000000 / LOWER_THRESHOLD_MHZ >
	     CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);
BUILD_ASSERT(1000000 / UPPER_THRESHOLD_MHZ <
	     CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);
#else
#define LOWER_THRESHOLD_MHZ 0
#define UPPER_THRESHOLD_MHZ 0
#endif

#ifndef CONFIG_ACCEL_FORCE_MODE_MASK
#define CONFIG_ACCEL_FORCE_MODE_MASK 0
#endif

TEST_F(MotionSenseInterruptToggle, TestDisableInterrupt)
{
	if (CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS == 0) {
		/* Runtime config is off for threshold rate */
		GTEST_SKIP();
	}

	pw::StringBuffer<128> cmd;
	cros::shell::Shell shell(cmd);

	for (auto *s = begin; s < end; ++s) {
		if (s->drv->enable_interrupt == nullptr) {
			continue;
		}
		if ((CONFIG_ACCEL_FORCE_MODE_MASK & BIT(s - begin)) != 0) {
			continue;
		}
		SetUp();
		/* Verify state */
		ASSERT_EQ(s->state, SENSOR_READY);

		/* Try setting the ODR below the threshold */
		ASSERT_TRUE((shell << "accelrate " << (s - begin) << " "
				   << LOWER_THRESHOLD_MHZ << std::endl)
				    .status()
				    .ok());
		/* Sleep so the console command runs */
		k_msleep(2 * CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);
		/* Flush the FIFO */
		motion_sense_fifo_reset();
		/* Sleep so force mode sensors can sample */
		k_msleep(2 * CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);

		/* Verify we got data from the right sensor */
		VerifyHasData([s](const auto &data) -> bool {
			return data.sensor_num == s - motion_sensors;
		});

		/* Try setting the ODR above the threshold */
		ASSERT_TRUE((shell << "accelrate " << (s - begin) << " "
			     << UPPER_THRESHOLD_MHZ << std::endl)
			    .status().ok());
		/* Sleep so the console command runs */
		k_msleep(2 * CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);
		/* Flush the FIFO */
		motion_sense_fifo_reset();
		/* Sleep so force mode sensors can sample */
		k_msleep(2 * CONFIG_ACCEL_FORCE_MODE_THRESHOLD_RATE_MS);
		/* Verify none of the data has data from this sensor */
		VerifyEachData([s](const auto &data) -> bool {
			return data.sensor_num != s - motion_sensors;
		});
		TearDown();
	}
}
