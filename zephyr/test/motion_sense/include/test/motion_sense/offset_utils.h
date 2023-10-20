/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <zephyr/drivers/emul.h>
#include <zephyr/drivers/emul_sensor.h>
#include <zephyr/drivers/sensor.h>

#include <gtest/gtest.h>

__maybe_unused static int get_accel_offset_epsilon(const struct emul *emul)
{
	q31_t min;
	q31_t max;
	q31_t epsilon;
	int8_t epsilon_shift;
	EXPECT_EQ(0, emul_sensor_backend_get_attribute_metadata(
			     emul, SENSOR_CHAN_ACCEL_XYZ, SENSOR_ATTR_OFFSET,
			     &min, &max, &epsilon, &epsilon_shift));

	int64_t intermediate = (int64_t)epsilon * INT64_C(1000);
	if (epsilon_shift >= 0) {
		intermediate <<= epsilon_shift;
	} else {
		intermediate >>= -epsilon_shift;
	}
	return DIV_ROUND_UP(intermediate,
			    (int64_t)((int64_t)INT32_MAX * 9.80665f));
}

__maybe_unused static int get_gyro_offset_epsilon(const struct emul *emul)
{
	q31_t min;
	q31_t max;
	q31_t epsilon;
	int8_t epsilon_shift;
	EXPECT_EQ(0, emul_sensor_backend_get_attribute_metadata(
			     emul, SENSOR_CHAN_GYRO_XYZ, SENSOR_ATTR_OFFSET,
			     &min, &max, &epsilon, &epsilon_shift));

	int64_t intermediate =
		(int64_t)epsilon * INT64_C(180) * INT64_C(1000) / 3.1415926f;
	if (epsilon_shift >= 0) {
		intermediate <<= epsilon_shift;
	} else {
		intermediate >>= -epsilon_shift;
	}
	return DIV_ROUND_UP(intermediate, INT32_MAX);
}
