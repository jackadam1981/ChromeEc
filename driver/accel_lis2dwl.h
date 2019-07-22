/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LIS2DWL accelerometer module for Chrome EC */

#ifndef __CROS_EC_ACCEL_LIS2DWL_H
#define __CROS_EC_ACCEL_LIS2DWL_H

#include "driver/stm_mems_common.h"

/*
 * LIS2DWL:
 *
 * 7-bit address is 0011 00X b. Where 'X' is determined
 * by the voltage on the ADDR pin
 */
#define LIS2DWL_ADDR0_FLAGS	0x18
#define LIS2DWL_ADDR1_FLAGS	0x19

/* Who Am I  */
#define LIS2DWL_WHO_AM_I_REG	0x0f
#define LIS2DWL_WHO_AM_I	0x44

/* COMMON DEFINE FOR ACCEL SENSOR */
#define LIS2DWL_EN_BIT		0x01
#define LIS2DWL_DIS_BIT		0x00

#define LIS2DWL_INT2_ON_INT1_ADDR	0x3f
#define LIS2DWL_INT2_ON_INT1_MASK	0x40

#define LIS2DWL_OUT_X_L_ADDR	0x28

#define LIS2DWL_CTRL1_ADDR	0x20
#define LIS2DWL_MODE_MASK	0xc0
#define LIS2DWL_MODE_POS	0x02
#define LIS2DWL_MODE_LOW_POWER	0x00
#define LIS2DWL_MODE_HIGH_PERF	0x01

#define LIS2DWL_CTRL2_ADDR	0x21
#define LIS2DWL_IF_ADD_INC_MASK 0x04
#define LIS2DWL_BDU_MASK	0x08

#define LIS2DWL_CTRL3_ADDR	0x22
#define LIS2DWL_CTRL3_RESET_VAL 0x00

#define LIS2DWL_CTRL4_ADDR	0x23
#define LIS2DWL_CTRL4_RESET_VAL 0x00

#define LIS2DWL_CTRL5_ADDR	0x24
#define LIS2DWL_CTRL5_RESET_VAL 0x00

#define LIS2DWL_CTRL6_ADDR	0x25
#define LIS2DWL_CTRL6_RESET_VAL 0x00
#define LIS2DWL_FULL_SCALE_MASK	0x30

#define LIS2DWL_STATUS_REG	0x27
#define LIS2DWL_STS_XLDA_UP	0x01

#define LIS2DWL_FS_2G_VAL       0x00
#define LIS2DWL_FS_4G_VAL       0x01
#define LIS2DWL_FS_8G_VAL       0x02
#define LIS2DWL_FS_16G_VAL      0x03

/* Interrupt source status register */
#define LIS2DWL_INT_SRC_REG	0x3B

/* Output data rate Mask register */
#define LIS2DWL_ACC_ODR_MASK	0xf0

/* Acc data rate */
/* In High Performance power mode, both reg value 0x00 and 0x01
 * wil get a ODR 12.5Hz
 */
enum lis2dwl_odr {
	LIS2DWL_ODR_0HZ_VAL = 0,
	LIS2DWL_ODR_13HZ_VAL,
	LIS2DWL_ODR_13HZ_VAL_DUP,
	LIS2DWL_ODR_25HZ_VAL,
	LIS2DWL_ODR_50HZ_VAL,
	LIS2DWL_ODR_100HZ_VAL,
	LIS2DWL_ODR_200HZ_VAL,
	LIS2DWL_ODR_400HZ_VAL,
	LIS2DWL_ODR_LIST_NUM
};

/* Absolute maximum rate for sensor */
#define LIS2DWL_ODR_MIN_VAL		12500
#define LIS2DWL_ODR_MAX_VAL \
	MOTION_MAX_SENSOR_FREQUENCY(400000, 25000)

/* Return ODR reg value based on data rate set */
#define LIS2DWL_ODR_TO_REG(_odr) \
	((_odr <= 12500) ? LIS2DWL_ODR_13HZ_VAL : \
	(((31 - __builtin_clz(_odr / 25000))) + 3))

/* Return ODR real value normalized to sensor capabilities */
#define LIS2DWL_ODR_TO_NORMALIZE(_odr) \
	((_odr <= 12500) ? 12500 : \
	(25000 * (1 << (31 - __builtin_clz(_odr / 25000)))))

/* Return ODR real value normalized to sensor capabilities from reg value */
#define LIS2DWL_REG_TO_NORMALIZE(_reg) \
	((_reg == LIS2DWL_ODR_13HZ_VAL) ? 12500 : \
	(_reg == LIS2DWL_ODR_13HZ_VAL_DUP) ? 12500 : \
	(25000 * (1 << (_reg - 3))))

/* Full scale range Mask register */
#define LIS2DWL_FS_MASK		0x30

/* FS reg value from Full Scale */
#define LIS2DWL_FS_TO_REG(_fs) (__fls(_fs) - 1)

/*
 * Sensor resolution in number of bits
 *
 * lis2dwl has variable precision (12/14 bits) depending Power Mode
 * selected, here Only High Performance Power mode supported (14 bits).
 *
 */
#define LIS2DWL_RESOLUTION       14

extern const struct accelgyro_drv lis2dwl_drv;

#endif /* __CROS_EC_ACCEL_LIS2DWL_H */
