/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * CREATE_MOTION_SENSOR is the macro to create an entry in
 * motion_sensors array. The macro gets value of compatible property of
 * the sensor in device tree. It creates the corresponding
 * motion_sense_t entry for motion_sensors array.
 */

/*
 * Here, we call CREATE_MOTION_SENSOR for each BMI260_accel
 * instance (compatible = "cros-ec,drv-bmi260-accel")
 * in device tree to create motion_sensor_t entry for the instance.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_bmi260_accel, MOTIONSENSE_CHIP_BMI260,	\
		MOTIONSENSE_TYPE_ACCEL,	bmi260_drv,			\
		BMI_ACCEL_MIN_FREQ, BMI_ACCEL_MAX_FREQ)

/*
 * Here, we call CREATE_MOTION_SENSOR for each BMI260_gyro
 * instance (compatible = "cros-ec,drv-bmi260-gyro")
 * in device tree to create motion_sensor_t entry for the instance.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_bmi260_gyro, MOTIONSENSE_CHIP_BMI260,	\
		MOTIONSENSE_TYPE_GYRO, bmi260_drv,			\
		BMI_GYRO_MIN_FREQ, BMI_GYRO_MAX_FREQ)
