/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSM accelerometer and gyro module for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSM_H
#define __CROS_EC_ACCELGYRO_LSM6DSM_H

#include "accelgyro.h"
#include "task.h"

#define LSM6DSM_I2C_ADDR(__x)		(__x << 1)

/*
 * 7-bit address is 110101Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
#define LSM6DSM_ADDR0             	LSM6DSM_I2C_ADDR(0x6a)
#define LSM6DSM_ADDR1             	LSM6DSM_I2C_ADDR(0x6b)

/* who am I  */
#define LSM6DSM_WHO_AM_I_REG		0x0f
#define LSM6DSM_WHO_AM_I          	0x6a

#define LSM6DSM_OUT_XYZ_SIZE 		6
/* Sensor Software Reset Bit */
#define LSM6DSM_RESET_ADDR		0x12
#define LSM6DSM_RESET_MASK		0x01

/* COMMON DEFINE FOR ACCEL-GYRO SENSORS */
#define LSM6DSM_EN_BIT			0x01
#define LSM6DSM_DIS_BIT			0x00

#define LSM6DSM_LIR_ADDR		0x58
#define LSM6DSM_LIR_MASK		0x01

#define LSM6DSM_BDU_ADDR		0x12
#define LSM6DSM_BDU_MASK		0x40

#define LSM6DSM_INT2_ON_INT1_ADDR	0x13
#define LSM6DSM_INT2_ON_INT1_MASK	0x20

#define LSM6DSM_GYRO_OUT_X_L_ADDR	0x22
#define LSM6DSM_ACCEL_OUT_X_L_ADDR	0x28

#define LSM6DSM_CTRL1_ADDR		0x10
#define LSM6DSM_CTRL2_ADDR		0x11
#define LSM6DSM_CTRL3_ADDR		0x12
#define LSM6DSM_CTRL6_ADDR		0x15
#define LSM6DSM_CTRL7_ADDR		0x16

#define LSM6DSM_STATUS_REG		0x1e

/* Output data rate registers */
#define LSM6DSM_ACC_ODR_ADDR		LSM6DSM_CTRL1_ADDR
#define LSM6DSM_ACC_ODR_MASK		0xf0
#define LSM6DSM_GYR_ODR_ADDR		LSM6DSM_CTRL2_ADDR
#define LSM6DSM_GYR_ODR_MASK		0xf0

/* Acc/Gyro data rate */
enum lsm6dsm_odr {
	LSM6DSM_ODR_POWER_OFF_VAL = 0x00,
	LSM6DSM_ODR_13HZ_VAL,
	LSM6DSM_ODR_26HZ_VAL,
	LSM6DSM_ODR_52HZ_VAL,
	LSM6DSM_ODR_104HZ_VAL,
	LSM6DSM_ODR_208HZ_VAL,
	LSM6DSM_ODR_416HZ_VAL,
#ifdef LSM6DSM_HIFI
	LSM6DSM_ODR_833HZ_VAL,
	LSM6DSM_ODR_1660HZ_VAL,
	LSM6DSM_ODR_3330HZ_VAL,
	LSM6DSM_ODR_6660HZ_VAL,
#endif /* LSM6DSM_HIFI */
	LSM6DSM_ODR_LIST_NUM
};

#define LSM6DSM_FS_LIST_NUM		4

/* CUSTOM VALUES FOR ACCEL SENSOR */
#define LSM6DSM_ACCEL_FS_ADDR		0x10
#define LSM6DSM_ACCEL_FS_MASK		0x0c
#define LSM6DSM_ACCEL_FS_2G_VAL		0x00
#define LSM6DSM_ACCEL_FS_4G_VAL		0x02
#define LSM6DSM_ACCEL_FS_8G_VAL		0x03
#define LSM6DSM_ACCEL_FS_16G_VAL	0x01
#define LSM6DSM_ACCEL_FS_2G_GAIN	61
#define LSM6DSM_ACCEL_FS_4G_GAIN	122
#define LSM6DSM_ACCEL_FS_8G_GAIN	244
#define LSM6DSM_ACCEL_FS_16G_GAIN	488

/* CUSTOM VALUES FOR GYRO SENSOR */
#define LSM6DSM_GYRO_ODR_ADDR		0x11
#define LSM6DSM_GYRO_ODR_MASK		0xf0
#define LSM6DSM_GYRO_FS_ADDR		0x11
#define LSM6DSM_GYRO_FS_MASK		0x0c
#define LSM6DSM_GYRO_FS_245_VAL		0x00
#define LSM6DSM_GYRO_FS_500_VAL		0x01
#define LSM6DSM_GYRO_FS_1000_VAL	0x02
#define LSM6DSM_GYRO_FS_2000_VAL	0x03
#define LSM6DSM_GYRO_FS_245_GAIN	8750
#define LSM6DSM_GYRO_FS_500_GAIN	17500
#define LSM6DSM_GYRO_FS_1000_GAIN	35000
#define LSM6DSM_GYRO_FS_2000_GAIN	70000

/*
 * Register      : STATUS_REG
 * Address       : 0X1e
 */
enum lsm6dsm_status {
	LSM6DSM_STS_DOWN = 0x00,
	LSM6DSM_STS_XLDA_UP = 0x01,
	LSM6DSM_STS_GDA_UP = 0x02
};

enum lsm6dsm_sensors {
	LSM6DSM_ACCEL = 0,
	LSM6DSM_GYRO,
	LSM6DSM_SENSORS_NUMB
};

#define LSM6DSM_STS_XLDA_MASK		0x01
#define LSM6DSM_STS_GDA_MASK		0x02

/* Sensor resolution in number of bits. This sensor has fixed resolution. */
#define LSM6DSM_RESOLUTION      	16

extern const struct accelgyro_drv lsm6dsm_drv;

struct lsm6dsm_data {
	struct accelgyro_saved_data_t base;
	int16_t offset[3];
	uint8_t fs_id;
};
extern struct lsm6dsm_data lsm6dsm_a_data;
extern struct lsm6dsm_data lsm6dsm_g_data;

#endif /* __CROS_EC_ACCELGYRO_LSM6DSM_H */
