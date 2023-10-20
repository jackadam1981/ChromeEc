/* Copyright 2023 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#define BMI160_NODE           DT_NODELABEL(accel_bmi160)
#define BMI160_ACC_SENSOR_ID  SENSOR_ID(DT_NODELABEL(ms_bmi160_accel))
#define BMI160_GYRO_SENSOR_ID SENSOR_ID(DT_NODELABEL(ms_bmi160_gyro))
