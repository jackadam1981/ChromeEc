/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * CREATE_MOTION_SENSOR which is defined in motionsense_sensros.c is
 * the macro to create an entry in motion_sensors array.
 * The macro gets value of compatible property of
 * the sensor in device tree and sensor specific values like chip ID,
 * type of sensor, name of driver, default min/max frequency.
 * Then using the values, it creates the corresponding motion_sense_t entry
 * in motion_sensors array.
 */

/*
 * Here, we call CREATE_MOTION_SENSOR to create a motion_sensor_t entry
 * for each BMI260_accel instance(compatible = "cros-ec,drv-bmi260-accel")
 * in device tree.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_bmi260_accel, MOTIONSENSE_CHIP_BMI260,	\
		MOTIONSENSE_TYPE_ACCEL,	bmi260_drv,			\
		BMI_ACCEL_MIN_FREQ, BMI_ACCEL_MAX_FREQ)

/*
 * Here, we call CREATE_MOTION_SENSOR to create a motion_sensor_t entry
 * for each BMI260_gyro instance (compatible = "cros-ec,drv-bmi260-gyro")
 * in device tree.
 */
CREATE_MOTION_SENSOR(cros_ec_drv_bmi260_gyro, MOTIONSENSE_CHIP_BMI260,	\
		MOTIONSENSE_TYPE_GYRO, bmi260_drv,			\
		BMI_GYRO_MIN_FREQ, BMI_GYRO_MAX_FREQ)
