/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LSM6DSO Accel and Gyro driver for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_LSM6DSO_H
#define __CROS_EC_ACCELGYRO_LSM6DSO_H

#include "stm_mems_common.h"

#define LSM6DSO_I2C_ADDR(__x)		(__x << 1)

#define min(x, y)	(x < y ? x : y)

/*
 * 7-bit address is 110101xb. Where 'x' is determined
 * by the voltage on the ADDR pin
 */
#define LSM6DSO_ADDR0			LSM6DSO_I2C_ADDR(0x6a)
#define LSM6DSO_ADDR1			LSM6DSO_I2C_ADDR(0x6b)

/* Access to embedded sensor hub register bank */
#define LSM6DSO_FUNC_CFG_ACC_ADDR	0x01
#define LSM6DSO_FUNC_CFG_EN		0x80
#define LSM6DSO_FUNC_CFG_EN_B		0x20

/* Who Am I */
#define LSM6DSO_WHO_AM_I_REG		0x0f
#define LSM6DSO_WHO_AM_I		0x6c

/* COMMON DEFINE FOR ACCEL-GYRO SENSORS */
#define LSM6DSO_EN_BIT			0x01
#define LSM6DSO_DIS_BIT			0x00

#define LSM6DSO_GYRO_OUT_X_L_ADDR	0x22
#define LSM6DSO_ACCEL_OUT_X_L_ADDR	0x28

#define LSM6DSO_CTRL1_ADDR		0x10
#define LSM6DSO_CTRL2_ADDR		0x11
#define LSM6DSO_CTRL3_ADDR		0x12
#define LSM6DSO_SW_RESET		0x01
#define LSM6DSO_IF_INC			0x04
#define LSM6DSO_PP_OD			0x10
#define LSM6DSO_H_L_ACTIVE		0x20
#define LSM6DSO_BDU			0x40

#define LSM6DSO_CTRL4_ADDR		0x13
#define LSM6DSO_INT2_ON_INT1_MASK	0x20

#define LSM6DSO_CTRL5_ADDR		0x14
#define LSM6DSO_CTRL6_ADDR		0x15
#define LSM6DSO_CTRL7_ADDR		0x16
#define LSM6DSO_CTRL8_ADDR		0x17
#define LSM6DSO_CTRL9_ADDR		0x18

#define LSM6DSO_CTRL10_ADDR		0x19
#define LSM6DSO_TIMESTAMP_EN    	0x20

#define LSM6DSO_STATUS_REG		0x1e

/* Output data rate registers and masks */
#define LSM6DSO_ODR_REG(_sensor) \
	(LSM6DSO_CTRL1_ADDR + _sensor)
#define LSM6DSO_ODR_MASK		0xf0

/* Hardware FIFO size in byte */
#define LSM6DSO_MAX_FIFO_SIZE		4096
#define LSM6DSO_MAX_FIFO_LENGTH	(LSM6DSO_MAX_FIFO_SIZE / OUT_XYZ_SIZE)

/* FIFO decimator registers and bitmask */
#define LSM6DSO_FIFO_CTRL1_ADDR		0x07
#define LSM6DSO_FIFO_CTRL2_ADDR		0x08

#define LSM6DSO_FIFO_CTRL3_ADDR		0x09
#define LSM6DSO_FIFO_ODR_XL_MASK	0x0f
#define LSM6DSO_FIFO_ODR_G_MASK		0xf0

#define LSM6DSO_ODR_MASK		0xf0

#define LSM6DSO_FIFO_CTRL4_ADDR		0x0a
#define LSM6DSO_FIFO_MODE_MASK		0x07

#define LSM6DSO_INT1_CTRL		0x0d
#define LSM6DSO_INT2_CTRL		0x0e
#define LSM6DSO_INT_FIFO_TH		0x08
#define LSM6DSO_INT_FIFO_OVR		0x10
#define LSM6DSO_INT_FIFO_FULL		0x20

#define LSM6DSO_FIFO_STS1_ADDR		0x3a
#define LSM6DSO_FIFO_STS2_ADDR		0x3b
#define LSM6DSO_FIFO_DIFF_MASK		0x07ff

#define LSM6DSO_FIFO_FULL		0x2000
#define LSM6DSO_FIFO_DATA_OVR		0x4000
#define LSM6DSO_FIFO_WATERMARK		0x8000

/* Out data register */
#define LSM6DSO_FIFO_DATA_ADDR_TAG	0x78

/* Registers value for supported FIFO mode */
#define LSM6DSO_FIFO_MODE_BYPASS_VAL	0x00
#define LSM6DSO_FIFO_MODE_CONTINUOUS_VAL	0x06

/* Register values for Sensor Hub Slave 0 */
#define LSM6DSO_SLV0_ADD_ADDR		0x02
#define LSM6DSO_SLV0_ADDR_SHFT		1
#define LSM6DSO_SLV0_ADDR_MASK		0xfe
#define LSM6DSO_SLV0_RD_BIT		0x01

#define LSM6DSO_SLV0_SUBADD_ADDR	0x03

#define LSM6DSO_SLV0_CONFIG_ADDR	0x04
#define LSM6DSO_SLV0_SLV_RATE_SHFT	6
#define LSM6DSO_SLV0_SLV_RATE_MASK	0xc0
#define LSM6DSO_SLV0_AUX_SENS_SHFT	4
#define LSM6DSO_SLV0_AUX_SENS_MASK	0x30
#define LSM6DSO_SLV0_NUM_OPS_MASK	0x07

#define LSM6DSO_SLV1_CONFIG_ADDR	0x07
#define LSM6DSO_SLV0_WR_ONCE_MASK	0x20

#define LSM6DSO_SENSORHUB1_REG		0x2e

/* Registers value for sensor Hub */
#define LSM6DSO_FUNC_SRC1		0x53
#define LSM6DSO_SENSORHUB_END_OP	0x01

/* Define device available in FIFO pattern */
enum lsm6dso_dev_fifo {
	LSM6DSO_FIFO_DEV_INVALID = -1,
	LSM6DSO_FIFO_DEV_GYRO = 0,
	LSM6DSO_FIFO_DEV_ACCEL,
	LSM6DSO_FIFO_DEV_NUM,
};

#define LSM6DSO_SAMPLE_SIZE		6
#define LSM6DSO_TS_SAMPLE_SIZE		4
#define LSM6DSO_TAG_SIZE		1
#define LSM6DSO_FIFO_SAMPLE_SIZE	LSM6DSO_SAMPLE_SIZE + LSM6DSO_TAG_SIZE
#define LSM6DSO_MAX_FIFO_DEPTH		416

enum lsm6dso_tag_fifo {
	LSM6DSO_GYRO_TAG = 0x01,
	LSM6DSO_ACC_TAG = 0x02,
};

struct lsm6dso_fstatus {
	uint16_t len;
	uint16_t pattern;
};

/* Absolute maximum rate for acc and gyro sensors */
#define LSM6DSO_ODR_MIN_VAL		13000
#define LSM6DSO_ODR_MAX_VAL \
	MOTION_MAX_SENSOR_FREQUENCY(416000, LSM6DSO_ODR_MIN_VAL)

/* ODR reg value from selected data rate in mHz */
#define LSM6DSO_ODR_TO_REG(_odr) (__fls(_odr / LSM6DSO_ODR_MIN_VAL) + 1)

#define LSM6DSO_FIFO_ODR_TO_REG(_s) \
	(_s->type == MOTIONSENSE_TYPE_ACCEL ? LSM6DSO_FIFO_ODR_XL_MASK : \
	 LSM6DSO_FIFO_ODR_G_MASK)

/* normalized ODR value from selected data rate in mHz */
#define LSM6DSO_REG_TO_ODR(_reg) (LSM6DSO_ODR_MIN_VAL << (_reg - 1))

/* Full Scale range value and gain for Acc */
#define LSM6DSO_FS_LIST_NUM		4

#define LSM6DSO_ACCEL_FS_ADDR		0x10
#define LSM6DSO_ACCEL_FS_MASK		0x0c

#define LSM6DSO_ACCEL_FS_2G_VAL		0x00
#define LSM6DSO_ACCEL_FS_4G_VAL		0x02
#define LSM6DSO_ACCEL_FS_8G_VAL		0x03
#define LSM6DSO_ACCEL_FS_16G_VAL	0x01

#define LSM6DSO_ACCEL_FS_MAX_VAL	16

/* Accel Reg value from Full Scale */
#define LSM6DSO_ACCEL_FS_REG(_fs) \
	(_fs == 2 ? LSM6DSO_ACCEL_FS_2G_VAL : \
	_fs == 16 ? LSM6DSO_ACCEL_FS_16G_VAL : \
	__fls(_fs))

/* Accel normalized FS value from Full Scale */
#define LSM6DSO_ACCEL_NORMALIZE_FS(_fs) (1 << __fls(_fs))

/* Full Scale range value and gain for Gyro */
#define LSM6DSO_GYRO_FS_ADDR		0x11
#define LSM6DSO_GYRO_FS_MASK		0x0c

#define LSM6DSO_GYRO_FS_250_VAL		0x00
#define LSM6DSO_GYRO_FS_500_VAL		0x01
#define LSM6DSO_GYRO_FS_1000_VAL	0x02
#define LSM6DSO_GYRO_FS_2000_VAL	0x03

#define LSM6DSO_GYRO_FS_250_GAIN	8750
#define LSM6DSO_GYRO_FS_500_GAIN	17500
#define LSM6DSO_GYRO_FS_1000_GAIN	35000
#define LSM6DSO_GYRO_FS_2000_GAIN	70000

#define LSM6DSO_GYRO_FS_MAX_VAL		20000

/* Gyro FS Gain value from selected Full Scale */
#define LSM6DSO_GYRO_FS_GAIN(_fs) \
	(LSM6DSO_GYRO_FS_250_GAIN << __fls(_fs / 250))

/* Gyro FS Full Scale value from Gain */
#define LSM6DSO_GYRO_GAIN_FS(_gain) \
	(_gain == LSM6DSO_GYRO_FS_250_GAIN ? 250 : \
	500 << (30 - __builtin_clz(_gain / LSM6DSO_GYRO_FS_250_GAIN)))

/* Gyro Reg value from Full Scale */
#define LSM6DSO_GYRO_FS_REG(_fs) \
	__fls(_fs / 250)

/* Gyro normalized FS value from Full Scale: for Gyro Gains are not multiple */
#define LSM6DSO_GYRO_NORMALIZE_FS(_fs) \
	(_fs == 250 ? 250 : 500 << __fls(_fs / 500))

/* FS register address/mask for Acc/Gyro sensors */
#define LSM6DSO_RANGE_REG(_sensor)  (LSM6DSO_ACCEL_FS_ADDR + (_sensor))
#define LSM6DSO_RANGE_MASK  		0x0c

/* Status register bitmask for Acc/Gyro data ready */
enum lsm6dso_status {
	LSM6DSO_STS_DOWN = 0x00,
	LSM6DSO_STS_XLDA_UP = 0x01,
	LSM6DSO_STS_GDA_UP = 0x02
};

#define LSM6DSO_STS_XLDA_MASK		0x01
#define LSM6DSO_STS_GDA_MASK		0x02

/* Sensor resolution in number of bits: fixed 16 bit */
#define LSM6DSO_RESOLUTION      	16

extern const struct accelgyro_drv lsm6dso_drv;

void lsm6dso_interrupt(enum gpio_signal signal);

#endif /* __CROS_EC_ACCELGYRO_LSM6DSO_H */
