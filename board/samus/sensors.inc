/* -*- mode:c -*- */

#define MOTION_SENSOR_CHIPS(CHIP)			\
	CHIP(LSM6DS0, BASE)				\
	CHIP(KXCJ9,   LID)

/*
 * Note: lsm6ds0: supports accelerometer and gyro sensor
 * Requirement: accelerometer sensor must init before gyro sensor
 * DO NOT change the order of the following table.
 */
	/*      chip  location type  address,  active     rot_std_ref       default_odr default_range */
#define LSM6DS0_BASE_SENSOR_LIST(SENSOR)	\
	SENSOR(LSM6DS0, BASE,  ACCEL, ADDR1,   S0_S3_S5,  base_standard_ref, 119000,      2) \
	SENSOR(LSM6DS0, BASE,  GYRO,  ADDR1,   S0,        null,              119000,      2000)

#define KXCJ9_LID_SENSOR_LIST(SENSOR)			\
	SENSOR(KXCJ9,   LID,   ACCEL, ADDR0,   S0,        lid_standard_ref,  100000,      2)

#define SENSOR_LIST(SENSOR)			\
	LSM6DS0_BASE_SENSOR_LIST(SENSOR)	\
	KXCJ9_LID_SENSOR_LIST(SENSOR)
