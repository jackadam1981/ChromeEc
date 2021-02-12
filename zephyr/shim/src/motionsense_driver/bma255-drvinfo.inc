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

/* Here, we call CREATE_MOTION_SENSOR to create a motion_sensor_t entry
 * for each BMA255 instance(compatible = "cros-ec,drv-bma255")
 * in device tree.
 */

/* Create a motion_sense_t entry for each BMA255 instance in device tree */
CREATE_MOTION_SENSOR(cros_ec_drv_bma255, MOTIONSENSE_CHIP_BMA255,	\
		MOTIONSENSE_TYPE_ACCEL, bma2x2_accel_drv,		\
		BMA255_ACCEL_MIN_FREQ, BMA255_ACCEL_MAX_FREQ)
