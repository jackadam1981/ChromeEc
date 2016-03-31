/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Skylake Chrome Reference Design board-specific configuration */

#include "driver/accel_kionix.h"
#include "driver/accel_kxcj9.h"
#include "driver/gyro_l3gd20h.h"
#include "i2c.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

const struct i2c_port_t i2c_ports[]  = {
	{"console", ISH30_I2C0, 400,  GPIO_I2C0_0_SCL, GPIO_I2C0_0_SDA},
	{"sensors", ISH30_I2C1, 400,  GPIO_I2C0_1_SCL, GPIO_I2C0_1_SDA},
	{"unknown", ISH30_I2C2, 500,  GPIO_I2C1_SCL,   GPIO_I2C1_SDA},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);

#ifdef HAS_TASK_MOTIONSENSE
/* Four Motion sensors */
/* kxcj9 mutex and local/private data*/
static struct mutex g_kxcj9_mutex[2];
struct kionix_accel_data g_kxcj9_data[2] = {
	{.variant = KXCJ9},
	{.variant = KXCJ9},
};

#ifdef CONFIG_GYRO_L3GD20H
/* Gyro sensor */
/* l3gd20h mutex and local/private data*/
static struct mutex g_l3gd20h_mutex;
struct l3gd20_data g_l3gd20h_data;
#endif

/* Matrix to rotate accelrator into standard reference frame */
const matrix_3x3_t base_standard_ref = {
	{ 0, FLOAT_TO_FP(-1),  0},
	{FLOAT_TO_FP(-1),  0,  0},
	{ 0,  0,  FLOAT_TO_FP(-1)}
};

const matrix_3x3_t lid_standard_ref = {
	{FLOAT_TO_FP(-1), 0,  0},
	{ 0, FLOAT_TO_FP(1),  0},
	{ 0,  0, FLOAT_TO_FP(-1)}
};

struct motion_sensor_t motion_sensors[] = {
	{.name = "Base Accel",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_KXCJ9,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_BASE,
	 .drv = &kionix_accel_drv,
	 .mutex = &g_kxcj9_mutex[0],
	 .drv_data = &g_kxcj9_data[0],
	 .addr = KXCJ9_ADDR1,
	 .rot_standard_ref = &base_standard_ref,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 100000 | ROUND_UP_FLAG,
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
	{.name = "Lid Accel",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_KXCJ9,
	 .type = MOTIONSENSE_TYPE_ACCEL,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &kionix_accel_drv,
	 .mutex = &g_kxcj9_mutex[1],
	 .drv_data = &g_kxcj9_data[1],
	 .addr = KXCJ9_ADDR0,
	 .rot_standard_ref = &lid_standard_ref,
	 .default_range = 2,  /* g, enough for laptop. */
	 .config = {
		 /* AP: by default shutdown all sensors */
		 [SENSOR_CONFIG_AP] = {
			 .odr = 0,
			 .ec_rate = 0,
		 },
		 /* EC use accel for angle detection */
		 [SENSOR_CONFIG_EC_S0] = {
			 .odr = 100000 | ROUND_UP_FLAG,
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
#ifdef CONFIG_GYRO_L3GD20H
	{.name = "Lid Gyro",
	 .active_mask = SENSOR_ACTIVE_S0,
	 .chip = MOTIONSENSE_CHIP_L3GD20H,
	 .type = MOTIONSENSE_TYPE_GYRO,
	 .location = MOTIONSENSE_LOC_LID,
	 .drv = &l3gd20h_drv,
	 .mutex = &g_l3gd20h_mutex,
	 .drv_data = &g_l3gd20h_data,
	 .addr = L3GD20_ADDR1,
	 .rot_standard_ref = NULL,
	 .default_range = 2000,  /* DPS */
	 .config = {
		/* AP: by default shutdown all sensors */
		[SENSOR_CONFIG_AP] = {
			.odr = 0,
			.ec_rate = 0,
		},
		[SENSOR_CONFIG_EC_S0] = {
			.odr = 0
			.ec_rate = 0,
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
#endif
};
const unsigned int motion_sensor_count = ARRAY_SIZE(motion_sensors);
#endif
