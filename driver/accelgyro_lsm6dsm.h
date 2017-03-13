/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSM (also LSM6DSL) Accel and Gyro driver for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSM_H
#define __CROS_EC_ACCELGYRO_LSM6DSM_H

#include "accelgyro.h"
#include "stm_mems_common.h"

#define LSM6DSM_I2C_ADDR(__x)		(__x << 1)

/* 7-bit address is 110101xb. Where 'x' is determined
 * by the voltage on the ADDR pin
 */
#define LSM6DSM_ADDR0             	LSM6DSM_I2C_ADDR(0x6a)
#define LSM6DSM_ADDR1             	LSM6DSM_I2C_ADDR(0x6b)

/* Who Am I */
#define LSM6DSM_WHO_AM_I_REG		0x0f
#define LSM6DSM_WHO_AM_I          	0x6a

/* Sensor Software Reset Bit */
#define LSM6DSM_RESET_ADDR		0x12
#define LSM6DSM_RESET_MASK		0x01

/* COMMON DEFINE FOR ACCEL-GYRO SENSORS */
#define LSM6DSM_EN_BIT			0x01
#define LSM6DSM_DIS_BIT			0x00

#define LSM6DSM_BDU_ADDR		0x12
#define LSM6DSM_BDU_MASK		0x40

#define LSM6DSM_GYRO_OUT_X_L_ADDR	0x22
#define LSM6DSM_ACCEL_OUT_X_L_ADDR	0x28

#define LSM6DSM_CTRL1_ADDR		0x10
#define LSM6DSM_CTRL2_ADDR		0x11
#define LSM6DSM_CTRL3_ADDR		0x12
#define LSM6DSM_CTRL6_ADDR		0x15
#define LSM6DSM_CTRL7_ADDR		0x16

#define LSM6DSM_CTRL10_ADDR		0x19
#define LSM6DSM_FUNC_EN_MASK		0x04
#define LSM6DSM_SIG_MOT_MASK		0x01
#define LSM6DSM_FUNC_EN			0x04
#define LSM6DSM_SIG_MOT_EN		0x01

/* Master mode configuration register */
#define LSM6DSM_MASTER_CONFIG		0x1a
#define LSM6DSM_PASSTROUGH_MASK		0x1f
#define LSM6DSM_START_CONFIG		0x10
#define LSM6DSM_PULLUP_EN		0x08
#define LSM6DSM_PASSTROUGH_MODE		0x04
#define LSM6DSM_MASTER_ENABLE		0x01

#define LSM6DSM_TAP_SRC_ADDR		0x1c
#define LSM6DSM_STAP_DETECT		0x20
#define LSM6DSM_DTAP_DETECT		0x10

#define LSM6DSM_STATUS_REG		0x1e

#define LSM6DSM_FUNC_SRC1_ADDR		0x53
#define LSM6DSM_SIGN_MOTION_IA		0x40

#define LSM6DSM_LIR_ADDR		0x58
#define LSM6DSM_LIR_MASK		0x01
#define LSM6DSM_EN_INT			0x80
#define LSM6DSM_EN_TAP			0x0e
#define LSM6DSM_TAP_MASK		0x8e

#define LSM6DSM_TAP_THS_6D		0x59
#define LSM6DSM_D4D_EN_MASK		0x80
#define LSM6DSM_TAP_TH_MASK		0x1f

#define LSM6DSM_INT_DUR2_ADDR		0x5a
#define LSM6DSM_TAP_DUR_MASK		0xf0
#define LSM6DSM_TAP_QUIET_MASK		0x0c

#define LSM6DSM_WUP_THS_ADDR		0x5b
#define LSM6DSM_S_D_TAP_MASK		0x80
#define LSM6DSM_STAP_EN			0
#define LSM6DSM_DTAP_EN			1

#define LSM6DSM_MD1_CFG_ADDR		0x5e
#define LSM6DSM_INT1_STAP_MASK		0x40
#define LSM6DSM_INT1_DTAP_MASK		0x08

/* Output data rate registers and masks */
#define LSM6DSM_ODR_REG(_sensor) \
	(LSM6DSM_CTRL1_ADDR + _sensor)
#define LSM6DSM_ODR_MASK		0xf0

/* Common Acc/Gyro data rate */
enum lsm6dsm_odr {
	LSM6DSM_ODR_0HZ_VAL = 0,
	LSM6DSM_ODR_13HZ_VAL,
	LSM6DSM_ODR_26HZ_VAL,
	LSM6DSM_ODR_52HZ_VAL,
	LSM6DSM_ODR_104HZ_VAL,
	LSM6DSM_ODR_208HZ_VAL,
	LSM6DSM_ODR_416HZ_VAL,
	LSM6DSM_ODR_LIST_NUM
};

#ifdef CONFIG_MAG_LSM6DSM_LIS2MDL
#define PT_ENABLE			1
#define PT_DISABLE			0
#endif /* CONFIG_MAG_LSM6DSM_LIS2MDL */

/* Absolute maximum rate for acc and gyro sensors */
#define LSM6DSM_ODR_MIN_VAL		13000
#define LSM6DSM_ODR_MAX_VAL		416000

/* ODR reg value from selected data rate in mHz */
#define LSM6DSM_ODR_TO_REG(_odr) \
	(31 - __builtin_clz(_odr / LSM6DSM_ODR_MIN_VAL))

/* normalized ODR value from selected data rate in mHz */
#define LSM6DSM_ODR_TO_NORMALIZE(_odr) \
	(LSM6DSM_ODR_MIN_VAL << (31 - __builtin_clz(_odr/LSM6DSM_ODR_MIN_VAL)))

/* Full Scale range value and gain for Acc */
#define LSM6DSM_FS_LIST_NUM		4

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

#define LSM6DSM_ACCEL_FS_MAX_VAL	16

/* Accel Gain value from selected Full Scale */
#define LSM6DSM_ACCEL_FS_GAIN(_fs) \
	(_fs == 16 ? LSM6DSM_ACCEL_FS_16G_GAIN : \
	LSM6DSM_ACCEL_FS_2G_GAIN << (31 - __builtin_clz(_fs / 2)))

/* Accel FS Full Scale value from Gain */
#define LSM6DSM_ACCEL_GAIN_FS(_gain) \
	(1 << (32 - __builtin_clz(_gain / LSM6DSM_ACCEL_FS_2G_GAIN)))

/* Accel Reg value from Full Scale */
#define LSM6DSM_ACCEL_FS_REG(_fs) \
	(_fs == 2 ? LSM6DSM_ACCEL_FS_2G_VAL : \
	_fs == 16 ? LSM6DSM_ACCEL_FS_16G_VAL : \
	(32 - __builtin_clz(_fs / 2)))

/* Accel normalized FS value from Full Scale */
#define LSM6DSM_ACCEL_NORMALIZE_FS(_fs) \
	(1 << (32 - __builtin_clz(_fs / 2)))

/* Full Scale range value and gain for Gyro */
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

#define LSM6DSM_GYRO_FS_MAX_VAL		20000

/* Gyro FS Gain value from selected Full Scale */
#define LSM6DSM_GYRO_FS_GAIN(_fs) \
	(LSM6DSM_GYRO_FS_245_GAIN << (31 - __builtin_clz(_fs / 245)))

/* Gyro FS Full Scale value from Gain */
#define LSM6DSM_GYRO_GAIN_FS(_gain) \
	(_gain == LSM6DSM_GYRO_FS_245_GAIN ? 245 : \
	500 << (30 - __builtin_clz(_gain / LSM6DSM_GYRO_FS_245_GAIN)))

/* Gyro Reg value from Full Scale */
#define LSM6DSM_GYRO_FS_REG(_fs) \
	((31 - __builtin_clz(_fs / 245)))

/* Gyro normalized FS value from Full Scale: for Gyro Gains are not multiple */
#define LSM6DSM_GYRO_NORMALIZE_FS(_fs) \
	(_fs == 245 ? 245 : 500 << (31 - __builtin_clz(_fs / 500)))

/* FS register address/mask for Acc/Gyro sensors */
#define LSM6DSM_RANGE_REG(_sensor)  (LSM6DSM_ACCEL_FS_ADDR + (_sensor))
#define LSM6DSM_RANGE_MASK  		0x0c

/* Status register bitmask for Acc/Gyro data ready */
enum lsm6dsm_status {
	LSM6DSM_STS_DOWN = 0x00,
	LSM6DSM_STS_XLDA_UP = 0x01,
	LSM6DSM_STS_GDA_UP = 0x02
};

#define LSM6DSM_STS_XLDA_MASK		0x01
#define LSM6DSM_STS_GDA_MASK		0x02

/* Sensor resolution in number of bits
 * This sensor has fixed 16 bit resolution
 */
#define LSM6DSM_RESOLUTION      	16

extern const struct accelgyro_drv lsm6dsm_drv;

#endif /* __CROS_EC_ACCELGYRO_LSM6DSM_H */
