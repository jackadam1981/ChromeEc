/* Copyright 2024 The ChromiumOS Authors
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

#include <zephyr/logging/log.h>
#include <zephyr/shell/shell_dummy.h>

#include "test_lib/shell/shell.hh"
#include <gtest/gtest.h>
#include <iostream>

LOG_MODULE_REGISTER(interrupt_toggle);

namespace
{

/**
 * @brief a base Matcher to use with EXPECT_TRUE or ASSERT_TRUE
 */
template <typename T> class Matcher {
    public:
	/**
	 * @param predicate Function which can evaluate to a boolean
	 */
	explicit Matcher(pw::Function<bool(const T &)> &&predicate)
		: predicate_(std::move(predicate))
	{
	}

	/**
	 * Boolean conversion function, child classes must implement this.
	 *
	 * @return true if the matcher passed
	 */
	virtual operator bool() = 0;

    protected:
	pw::Function<bool(const T &)> predicate_;
};

/**
 * Check if a list contains at least 1 instance of a successful predicate
 */
template <typename T> class Contains : public Matcher<T> {
    public:
	Contains(pw::Function<bool(T &)> generator,
		 pw::Function<bool(const T &)> predicate)
		: Matcher<T>(std::move(predicate))
		, generator_(std::move(generator))
	{
	}

	operator bool() override
	{
		T data;

		while (generator_(data)) {
			if (this->predicate_(data)) {
				return true;
			}
		}
		return false;
	}

    private:
	pw::Function<bool(T &)> generator_;
};

/**
 * Check that all elements of a list pass a predicate
 */
template <typename T> class Each : public Matcher<T> {
    public:
	Each(pw::Function<bool(T &)> generator,
	     pw::Function<bool(const T &)> predicate)
		: Matcher<T>(std::move(predicate))
		, generator_(std::move(generator))
	{
	}

	operator bool() override
	{
		T data;

		while (generator_(data)) {
			if (!this->predicate_(data)) {
				return false;
			}
		}
		return true;
	}

    private:
	pw::Function<bool(T &)> generator_;
};

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
			for (int cfg = static_cast<int>(SENSOR_CONFIG_AP);
			     cfg < static_cast<int>(SENSOR_CONFIG_MAX); ++cfg) {
				s->config[cfg].odr = 0;
				s->config[cfg].ec_rate = 0;
			}
		}
		motion_sense_fifo_reset();
	}

	/**
	 * Check that motion_sense_fifo has one instance matching a predicate
	 *
	 * @param predicate What to check for
	 */
	void VerifyHasData(
		pw::Function<bool(const struct ec_response_motion_sensor_data &)>
			predicate)
	{
		EXPECT_TRUE(Contains<struct ec_response_motion_sensor_data>(
			generator(), std::move(predicate)));
		motion_sense_fifo_reset();
	}

	/**
	 * Check that all of motion_sense_fifo's entries match a predicate
	 *
	 * @param predicate What to check for
	 */
	void VerifyEachData(
		pw::Function<bool(const struct ec_response_motion_sensor_data &)>
			predicate)
	{
		EXPECT_TRUE(Each<struct ec_response_motion_sensor_data>(
			generator(), std::move(predicate)));
	}

	motion_sensor_t *const begin = &motion_sensors[0];
	motion_sensor_t *const end = &motion_sensors[motion_sensor_count];

    private:
	pw::Function<bool(struct ec_response_motion_sensor_data &)> generator()
	{
		return [](auto &data) {
			uint16_t out_size;
			return motion_sense_fifo_read(sizeof(data), 1, &data,
						      &out_size) > 0;
		};
	}
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

	/* Setup the shell */
	pw::StringBuffer<128> cmd;
	cros::shell::Shell shell(cmd);

	/*
	 * pw_unit_test doesn't yet support TEST_P parameterized tests, so for
	 * now we'll need to wrap this up with a loop and mock up the internals.
	 */
	for (auto *s = begin; s < end; ++s) {
		if (s->drv->enable_interrupt == nullptr) {
			LOG_DBG("Skipping sensor [%d] %s, no enable_interrupt function",
				s - begin, s->name);
			continue;
		}
		if ((CONFIG_ACCEL_FORCE_MODE_MASK & BIT(s - begin)) != 0) {
			LOG_DBG("Skipping sensor [%d] %s, sensor is ALWAYS in force mode",
				s - begin, s->name);
			continue;
		}

		/* Run SetUp() for each sensor iteration */
		SetUp();

		LOG_INF("Running test for [%d] %s", s - begin, s->name);

		/* Verify state */
		EXPECT_EQ(s->state, SENSOR_READY);

		/* Try setting the ODR below the threshold */
		EXPECT_TRUE((shell << "accelrate " << (s - begin) << " "
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
		EXPECT_TRUE((shell << "accelrate " << (s - begin) << " "
				   << UPPER_THRESHOLD_MHZ << std::endl)
				    .status()
				    .ok());

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

		/* Run the TearDown function for each sensor */
		TearDown();
	}
}

} /* namespace */
