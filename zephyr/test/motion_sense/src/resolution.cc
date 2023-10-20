/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "accelgyro.h"
#include "bmi160.h"
#include "emul_bmi160.h"
#include "math_util.h"
#include "test/motion_sense/fixtures.h"

#include <gtest/gtest.h>

TEST_F(Bmi160, Accel_GetResolution)
{
	EXPECT_EQ(16, ms_accel->drv->get_resolution(ms_accel));
}

TEST_F(Bmi160, Gyro_GetResolution)
{
	EXPECT_EQ(16, ms_gyro->drv->get_resolution(ms_gyro));
}
