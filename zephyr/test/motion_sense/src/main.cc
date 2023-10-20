/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "motion_sense.h"
#include "hooks.h"

#include <zephyr/kernel.h>

#include <gtest/gtest.h>
#include <pw_unit_test/logging_event_handler.h>
#include <pw_log/log.h>

static void main_init_hook()
{
	testing::InitGoogleTest(nullptr, nullptr);
	pw::unit_test::LoggingEventHandler handler;
	pw::unit_test::RegisterEventHandler(&handler);
	RUN_ALL_TESTS();
}
DECLARE_HOOK(HOOK_INIT, main_init_hook, HOOK_PRIO_LAST);

TEST(MotionSense, CheckSensorCount)
{
	EXPECT_EQ(motion_sensor_count, 2);
}
