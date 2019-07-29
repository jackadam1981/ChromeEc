/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPS22HH pressure/temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_LPS22HH_H
#define __CROS_EC_TEMP_SENSOR_LPS22HH_H

#include "driver/stm_mems_common.h"

#define LPS22HH_EN_BIT				1
#define LPS22HH_DIS_BIT				0

/*
 * 7-bit address is 000110Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
#define LPS22HH_ADDR0				0x5C
#define LPS22HH_ADDR1				0x5D

#define LPS22HH_REG_WHO_AM_I			0x0F
#define LPS22HH_VAL_WHO_AM_I			0xB3

#define LPS22HH_REG_CTRL_REG1			0x10
#define LPS22HH_MASK_ODR			0x70
#define LPS22HH_BDU_MASK			0x02
#define LPS22HH_BDU_EN				0x01

#define LPS22HH_REG_CTRL_REG2			0x11
#define LPS22HH_MASK_BOOT			0x80
#define LPS22HH_SWRESET				0x04
#define LPS22HH_IF_ADD_INC			0x10

#define LPS22HH_REG_CTRL_REG3			0x12

#define LPS22HH_STATUS_REG			0x27
#define LPS22HH_STATUS_P_DA			0x02
#define LPS22HH_STATUS_T_DA			0x01

#define LPS22HH_REG_PRESS_OUT_XL		0x28
#define LPS22HH_REG_PRESS_OUT_L			0x29
#define LPS22HH_REG_PRESS_OUT_H			0x2A

#define LPS22HH_REG_TEMP_OUT_L			0x2B
#define LPS22HH_REG_TEMP_OUT_H			0x2C

#define LPS22HH_POWER_DOWN			0
#define LPS22HH_ODR_1HZ				1
#define LPS22HH_ODR_10HZ			2
#define LPS22HH_ODR_25HZ			3
#define LPS22HH_ODR_50HZ			4
#define LPS22HH_ODR_75HZ			5
#define LPS22HH_ODR_100HZ			6
#define LPS22HH_ODR_200HZ			7

#define LPS22HH_MAX_ODR				200000
#define LPS22HH_MIN_ODR				1000

#define LPS22HH_ODR_TO_REG(_odr) ( \
	((_odr) <= 1000) ? LPS22HH_ODR_1HZ : \
	((_odr) <= 10000) ? LPS22HH_ODR_10HZ : \
	((31 - __builtin_clz((_odr) / 25000))) + 3)

/* Return ODR real value normalized to sensor capabilities. */
#define LPS22HH_ODR_TO_NORMALIZE(_odr) ( \
	((_odr) <= 1000) ? 1000 : ((_odr) <= 10000) ? 10000 : \
	(25000 * (1 << (31 - __builtin_clz((_odr) / 25000)))))

/* Pressure sensitivity: 4096 LSB/hPa. */
#define LPS22HH_P_SENSITIVITY			4096

/* Sensor resolution in number of bits. */
#define LPS22HH_RESOLUTION			24

extern const struct accelgyro_drv lps22hh_drv;

#endif  /* __CROS_EC_TEMP_SENSOR_LPS22HH_H */

