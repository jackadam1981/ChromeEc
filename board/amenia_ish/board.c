/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Skylake Chrome Reference Design board-specific configuration */

#include "driver/accel_kionix.h"
#include "driver/accel_kx022.h"
#include "driver/accelgyro_bmi160.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

const struct i2c_port_t i2c_ports[]  = {
	{"console", ISH_I2C0, 400,  GPIO_I2C0_0_SCL, GPIO_I2C0_0_SDA},
	{"sensors", ISH_I2C1, 400,  GPIO_I2C0_1_SCL, GPIO_I2C0_1_SDA},
	{"unknown", ISH_I2C2, 500,  GPIO_I2C1_SCL,   GPIO_I2C1_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef HAS_TASK_MOTIONSENSE
/* Mutexes */
static struct mutex g_lid_mutex;
static struct mutex g_base_mutex;

/* Matrix to rotate accelrator into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1),  0},
	{ FLOAT_TO_FP(1),  0,  0},
	{ 0,  0,  FLOAT_TO_FP(1)}
};

/* KX022 private data */
struct kionix_accel_data g_kx022_data;

/* Motion sensors */
struct motion_sensor_t motion_sensors[] = {
	/* Note: bmi160: supports accelerometer and gyro sensor
	 * Requirement: accelerometer sensor must init before gyro sensor
	 * DO NOT change the order of the following table.
	 */
	[LID_ACCEL] = {
		.name = "Lid Accel",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_BMI160,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bmi160_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bmi160_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMI160_ADDR0,
		.rot_standard_ref = NULL, /* Identity matrix. */
		.default_range = 2,  /* g, enough for laptop. */
		.config = {
			/* AP: by default use EC settings */
			[SENSOR_CONFIG_AP] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0
			},
		},
	},

	[LID_GYRO] = {
		.name = "Lid Gyro",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_BMI160,
		.type = MOTIONSENSE_TYPE_GYRO,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bmi160_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bmi160_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMI160_ADDR0,
		.default_range = 1000, /* dps */
		.rot_standard_ref = NULL, /* Identity Matrix. */
		.config = {
			/* AP: by default shutdown all sensors */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0,
			},
		},
	},

	[LID_MAG] = {
		.name = "Lid Mag",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_BMI160,
		.type = MOTIONSENSE_TYPE_MAG,
		.location = MOTIONSENSE_LOC_LID,
		.drv = &bmi160_drv,
		.mutex = &g_lid_mutex,
		.drv_data = &g_bmi160_data,
		.port = I2C_PORT_ACCEL,
		.addr = BMI160_ADDR0,
		.default_range = 1 << 11, /* 16LSB / uT, fixed */
		.rot_standard_ref = NULL, /* Identity Matrix. */
		.config = {
			/* AP: by default shutdown all sensors */
			[SENSOR_CONFIG_AP] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* EC does not need in S0 */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0,
			},
			/* Sensor off in S3/S5 */
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0,
			},
		},
	},

	[BASE_ACCEL] = {
		.name = "Base Accel",
		.active_mask = SENSOR_ACTIVE_S0,
		.chip = MOTIONSENSE_CHIP_KX022,
		.type = MOTIONSENSE_TYPE_ACCEL,
		.location = MOTIONSENSE_LOC_BASE,
		.drv = &kionix_accel_drv,
		.mutex = &g_base_mutex,
		.drv_data = &g_kx022_data,
		.port = I2C_PORT_ACCEL,
		.addr = KX022_ADDR1,
		.rot_standard_ref = &base_standard_ref, /* Identity matrix. */
		.default_range = 2, /* g, enough for laptop. */
		.config = {
			/* AP: by default use EC settings */
			[SENSOR_CONFIG_AP] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* EC use accel for angle detection */
			[SENSOR_CONFIG_EC_S0] = {
				.odr = 10000 | ROUND_UP_FLAG,
				.ec_rate = 100 * MSEC,
			},
			/* unused */
			[SENSOR_CONFIG_EC_S3] = {
				.odr = 0,
				.ec_rate = 0,
			},
			[SENSOR_CONFIG_EC_S5] = {
				.odr = 0,
				.ec_rate = 0,
			},
		},
	},
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);
#endif

