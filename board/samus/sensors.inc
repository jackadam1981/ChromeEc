/* -*- mode:c -*-
 *
 * Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * List each motion sensor chip along with its location.  Create a sensor list
 * in the format of CHIP_LOCATION_SENSOR_LIST and create a SENSOR_LIST composed
 * of each CHIP_LOCATION_SENSOR_LIST.
 */

#define MOTION_SENSOR_CHIPS(CHIP)	\
	CHIP(LSM6DS0, BASE)		\
	CHIP(KXCJ9,   LID)

#define SENSOR_LIST(SENSOR)			\
	LSM6DS0_BASE_SENSOR_LIST(SENSOR)	\
	KXCJ9_LID_SENSOR_LIST(SENSOR)

/*
 * Note: lsm6ds0: supports accelerometer and gyro sensor
 * Requirement: accelerometer sensor must init before gyro sensor
 * DO NOT change the order of the following table.
 */
	/*      chip  location type  address,  active     rot_std_ref       default_odr default_range */
#define LSM6DS0_BASE_SENSOR_LIST(SENSOR)							\
	SENSOR(LSM6DS0, BASE,  ACCEL, ADDR1,   S0_S3_S5,  base_standard_ref, 119000,      2)	\
	SENSOR(LSM6DS0, BASE,  GYRO,  ADDR1,   S0,        null,              119000,      2000)

#define KXCJ9_LID_SENSOR_LIST(SENSOR)								\
	SENSOR(KXCJ9,   LID,   ACCEL, ADDR0,   S0,        lid_standard_ref,  100000,      2)
