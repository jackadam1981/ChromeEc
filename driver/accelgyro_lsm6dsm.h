/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSM (also LSM6DSL) Accel and Gyro driver for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSM_H
#define __CROS_EC_ACCELGYRO_LSM6DSM_H

#include "accelgyro.h"
#include "stm_mems_common.h"

/* MIN_AZ - Calculate min between two integer (zero skip) */
static inline uint16_t MIN_AZ(int _a, int _b)
{
	if (_a < _b) {
		if (_a == 0)
			return _b;
		return _a;
	} else if (_b == 0)
		return _a;

	return _b;
}

#define LSM6DSM_I2C_ADDR(__x)		(__x << 1)

/*
 * 7-bit address is 110101xb. Where 'x' is determined
 * by the voltage on the ADDR pin
 */
#define LSM6DSM_ADDR0			LSM6DSM_I2C_ADDR(0x6a)
#define LSM6DSM_ADDR1			LSM6DSM_I2C_ADDR(0x6b)

/* Who Am I */
#define LSM6DSM_WHO_AM_I_REG		0x0f
#define LSM6DSM_WHO_AM_I		0x6a

/* Sensor Software Reset Bit */
#define LSM6DSM_RESET_ADDR		0x12
#define LSM6DSM_RESET_MASK		0x01

/* COMMON DEFINE FOR ACCEL-GYRO SENSORS */
#define LSM6DSM_EN_BIT			0x01
#define LSM6DSM_DIS_BIT			0x00

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

/* Output data rate registers and masks */
#define LSM6DSM_ODR_REG(_sensor) \
	(LSM6DSM_CTRL1_ADDR + _sensor)
#define LSM6DSM_ODR_MASK		0xf0

/* Common Acc/Gyro data rate */
enum lsm6dsm_odr {
	LSM6DSM_ODR_POWER_OFF_VAL = 0,
	LSM6DSM_ODR_13HZ_VAL,
	LSM6DSM_ODR_26HZ_VAL,
	LSM6DSM_ODR_52HZ_VAL,
	LSM6DSM_ODR_104HZ_VAL,
	LSM6DSM_ODR_208HZ_VAL,
	LSM6DSM_ODR_416HZ_VAL,
	LSM6DSM_ODR_LIST_NUM
};

#ifdef CONFIG_ACCEL_FIFO

/* Hardware FIFO size in byte */
#define LSM6DSM_MAX_FIFO_SIZE		4096
#define LSM6DSM_MAX_FIFO_LENGHT	(LSM6DSM_MAX_FIFO_SIZE / OUT_XYZ_SIZE)

/* FIFO decimator registers and bitmask */
#define LSM6DSM_FIFO_CTRL1_ADDR		0x06
#define LSM6DSM_FIFO_CTRL2_ADDR		0x07

#define LSM6DSM_FIFO_CTRL3_ADDR		0x08
#define LSM6DSM_FIFO_CTRL3_DEC_XL_MASK	0x07
#define LSM6DSM_FIFO_CTRL3_DEC_G_MASK	0x38

#define LSM6DSM_FIFO_CTRL4_ADDR		0x09
#define LSM6DSM_FIFO_CTRL4_DEC_M_MASK	0x07
#define LSM6DSM_FIFO_CTRL4_DEC_4_MASK	0x38

#define LSM6DSM_FIFO_DECIMATOR(_dec)\
	(_dec < 8 ? _dec : (2 + __builtin_ctz(_dec)))

#define LSM6DSM_FIFO_INT1_CTRL		0x0d
#define LSM6DSM_FTH_INT1_MASK		0x08
#define LSM6DSM_INT1_SIGN_MASK		0x40

#define LSM6DSM_FIFO_STS1_ADDR		0x3a
#define LSM6DSM_FIFO_STS2_ADDR		0x3b
#define LSM6DSM_FIFO_DIFF_MASK		0x07ff
#define LSM6DSM_FIFO_DATA_OVR		0x4000
#define LSM6DSM_FIFO_WATERMARK		0x80
#define LSM6DSM_FIFO_WMASK_L		0xff
#define LSM6DSM_FIFO_WMASK_H		0x07
#define LSM6DSM_FIFO_NODECIM		0x01

/* Out data register */
#define LSM6DSM_FIFO_DATA_ADDR		0x3e

enum fifo_mode {
/* Select FIFO supported mode:
 * BYPASS - Bypass FIFO
 * CONTINUOS - FIFO older data is replaced by new data
 * TODO: Other FIFO mode not supported
 */
	BYPASS = 0,
	CONTINUOS,
};

/* Registers value for supported FIFO mode */
#define LSM6DSM_FIFO_MODE_BYPASS_VAL	0x00
#define LSM6DSM_FIFO_MODE_CONTINUOS_VAL	0x06

#define LSM6DSM_FIFO_CTRL5_ADDR		0x0a
#define LSM6DSM_FIFO_CTRL5_MODE_MASK	0x07
#define LSM6DSM_FIFO_CTRL5_ODR_MASK	0x78

/* Registers value for sensor Hub */
#define LSM6DSM_FUNC_SRC1		0x53
#define LSM6DSM_SENSORHUB_END_OP	0x01

/* Define ODR FIFO values. Max value is max ODR for sensors
 * Value is limited to 416 Hz
 */
#define LSM6DSM_FIFO_ODR_MAX_VAL	LSM6DSM_ODR_416HZ_VAL
#define LSM6DSM_FIFO_ODR_OFF_VAL	LSM6DSM_ODR_POWER_OFF_VAL

/* Define device available in FIFO pattern */
enum dev_fifo {
	FIFO_DEV_GYRO = 0,
	FIFO_DEV_ACCEL,
	FIFO_DEV_NUM,
};

struct fstatus {
	uint16_t len;
	uint16_t pattern;
};

#endif /* CONFIG_ACCEL_FIFO */

/* Absolute maximum rate for acc and gyro sensors */
#define LSM6DSM_ODR_MIN_VAL		13000
#define LSM6DSM_ODR_MAX_VAL		416000

/* ODR reg value from selected data rate in mHz */
#define LSM6DSM_ODR_TO_REG(_odr) \
	__fls(_odr / LSM6DSM_ODR_MIN_VAL)

/* normalized ODR value from selected data rate in mHz */
#define LSM6DSM_ODR_TO_NORMALIZE(_odr) \
	(LSM6DSM_ODR_MIN_VAL << __fls(_odr/LSM6DSM_ODR_MIN_VAL))

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
	LSM6DSM_ACCEL_FS_2G_GAIN << __fls(_fs / 2))

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
	(LSM6DSM_GYRO_FS_245_GAIN << __fls(_fs / 245))

/* Gyro FS Full Scale value from Gain */
#define LSM6DSM_GYRO_GAIN_FS(_gain) \
	(_gain == LSM6DSM_GYRO_FS_245_GAIN ? 245 : \
	500 << (30 - __builtin_clz(_gain / LSM6DSM_GYRO_FS_245_GAIN)))

/* Gyro Reg value from Full Scale */
#define LSM6DSM_GYRO_FS_REG(_fs) \
	__fls(_fs / 245)

/* Gyro normalized FS value from Full Scale: for Gyro Gains are not multiple */
#define LSM6DSM_GYRO_NORMALIZE_FS(_fs) \
	(_fs == 245 ? 245 : 500 << __fls(_fs / 500))

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

/* Sensor resolution in number of bits: fixed 16 bit */
#define LSM6DSM_RESOLUTION      	16

extern const struct accelgyro_drv lsm6dsm_drv;

void lsm6dsm_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_ACCELGYRO_LSM6DSM_H */
