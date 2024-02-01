/* Copyright 2024 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "motion_sense.h"

#include <gtest/gtest.h>

class MotionSenseInterruptToggle : public ::testing::Test {};

//#define MK_SENSOR_ENTRY(inst, compat, measurement_type)                    \
//	TEST_F(MotionSenseInterruptToggle,                                 \
//	       Test##DT_INST(inst, compat)##_##measurement_type)           \
//	{                                                                  \
//		struct motion_sensor_t *s =                                \
//			&motion_sensors[SENSOR_ID(DT_INST(inst, compat))]; \
//                                                                           \
//		if (s->drv->enable_interrupt) {                            \
//			GTEST_SKIP();                                      \
//		}                                                          \
//	}
//
// #define CREATE_SENSOR_DATA
// #define CREATE_MOTION_SENSOR(compat, arg0, measurement_type, arg1, arg2,
// arg3) \
//	LISTIFY(DT_NUM_INST_STATUS_OKAY(compat), MK_SENSOR_TEST, (), compat,   \
//		measurement_type)
//
// #include "motionsense_driver/sensor_drv_list.inc"

TEST_F(MotionSenseInterruptToggle, TestDisableInterrupt)
{
	for (auto *s = &motion_sensors[0];
	     s < &motion_sensors[motion_sensor_count]; ++s) {
		if (s->drv->enable_interrupt == nullptr) {
			continue;
		}
		printk("Running test for '%s'\n", s->name);
	}
}
