/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include "accelgyro_bmi160_public.h"
#include "accelgyro_bmi_common_public.h"
#include "motion_sense.h"
#include "test/motion_sense/dts.h"
#include "test/motion_sense/offset_utils.h"

#include <gtest/gtest.h>

class Bmi160 : public ::testing::Test {
    protected:
	const struct emul *emul = EMUL_DT_GET(BMI160_NODE);
	struct motion_sensor_t *ms_accel =
		&motion_sensors[BMI160_ACC_SENSOR_ID];
	struct motion_sensor_t *ms_gyro =
		&motion_sensors[BMI160_GYRO_SENSOR_ID];

	void SetUp() override
	{
		ms_accel->rot_standard_ref = nullptr;
		ms_gyro->rot_standard_ref = nullptr;

		struct sensor_three_axis_attribute offset = {
			.shift = 0,
			.x = 0,
			.y = 0,
			.z = 0,
		};

		emul_sensor_backend_set_attribute(emul, SENSOR_CHAN_ACCEL_XYZ,
						  SENSOR_ATTR_OFFSET, &offset);
		emul_sensor_backend_set_attribute(emul, SENSOR_CHAN_GYRO_XYZ,
						  SENSOR_ATTR_OFFSET, &offset);

		ResetData(ms_accel);
		ResetData(ms_gyro);
	}

    private:
	void ResetData(const struct motion_sensor_t *s)
	{
		auto data = static_cast<struct bmi_drv_data_t *>(s->drv_data);
		auto saved_data = static_cast<struct accelgyro_saved_data_t *>(
			&data->saved_data[s->type]);

		saved_data->scale[0] = 1;
		saved_data->scale[1] = 1;
		saved_data->scale[2] = 1;
	}
};
