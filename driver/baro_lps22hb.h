/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* LPS22HB pressure/temperature sensor module for Chrome EC */

#ifndef __CROS_EC_TEMP_SENSOR_LPS22HB_H
#define __CROS_EC_TEMP_SENSOR_LPS22HB_H

#include "driver/stm_mems_common.h"

#define LPS22HB_SPI_READ			(1 << 7)
#define LPS22HB_EN_BIT				1
#define LPS22HB_DIS_BIT				0

/*
 * 7-bit address is 000110Xb. Where 'X' is determined
 * by the voltage on the ADDR pin.
 */
#define LPS22HB_I2C_ADDR(__x)			(__x << 1)
#define LPS22HB_ADDR0		LPS22HB_I2C_ADDR(0x5c)
#define LPS22HB_ADDR1		LPS22HB_I2C_ADDR(0x5d)

#define LPS22HB_REG_WHO_AM_I			0x0F
#define LPS22HB_VAL_WHO_AM_I			0xB1

#define LPS22HB_REG_CTRL_REG1			0x10
#define LPS22HB_MASK_ODR			0x70
#define LPS22HB_BDU_MASK			0x02
#define LPS22HB_BDU_EN				0x01

#define LPS22HB_REG_CTRL_REG2			0x11
#define LPS22HB_MASK_BOOT			0x80
#define LPS22HB_EN_FIFO_MASK			0x40
#define LPS22HB_SWRESET				0x04
#define LPS22HB_IF_ADD_INC			0x10

#define LPS22HB_REG_CTRL_REG3			0x12
#define LPS22HB_DRDY_INT			0x04
#define LPS22HB_DRDY_F_FTH			0x10
#define LPS22HB_INT_S_DRDY			0x00

#define LPS22HB_REG_CTRL_REG4			0x13

#define LPS22HB_FIFO_CTRL_REG			0x14
#define LPS22HB_FIFO_MODE_MASK			0xE0
#define LPS22HB_FIFO_WTM_MASK			0x1F

enum lps22hb_fmode {
	LPS22HB_FIFO_BYPASS_MODE = 0,
	LPS22HB_FIFO_FIFO_MODE,
	LPS22HB_FIFO_STREAM_MODE,
	LPS22HB_FMODE_UNUSED
};

#define LPS22HB_FIFO_STATUS_REG			0x26
#define LPS22HB_FIFO_UNREAD_MASK		0x3F
#define LPS22HB_FIFO_OVR_MASK			0x40
#define LPS22HB_FIFO_FTH_MASK			0x80

#define LPS22HB_STATUS_REG			0x27
#define LPS22HB_STATUS_P_DA			0x02
#define LPS22HB_STATUS_T_DA			0x01

#define LPS22HB_REG_PRESS_OUT_XL		0x28
#define LPS22HB_REG_PRESS_OUT_L			0x29
#define LPS22HB_REG_PRESS_OUT_H			0x2A

#define LPS22HB_REG_TEMP_OUT_L			0x2B
#define LPS22HB_REG_TEMP_OUT_H			0x2C

#define LPS22HB_POWER_DOWN			0
#define LPS22HB_ODR_1HZ				1
#define LPS22HB_ODR_10HZ			2
#define LPS22HB_ODR_25HZ			3
#define LPS22HB_ODR_50HZ			4
#define LPS22HB_ODR_75HZ			5

#define LPS22HB_MAX_ODR				75000
#define LPS22HB_MIN_ODR				1000

#define LPS22HB_ODR_TO_REG(_odr) ( \
	((_odr) <= 1000) ? LPS22HB_ODR_1HZ : \
	((_odr) <= 10000) ? LPS22HB_ODR_10HZ : \
	((31 - __builtin_clz((_odr) / 25000))) + 3)

/* Return ODR real value normalized to sensor capabilities. */
#define LPS22HB_ODR_TO_NORMALIZE(_odr) ( \
	((_odr) <= 1000) ? 1000 : ((_odr) <= 10000) ? 10000 : \
	(25000 * (1 << (31 - __builtin_clz((_odr) / 25000)))))

/* Pressure sensitivity: 4096 LSB/hPa. */
#define LPS22HB_P_SENSITIVITY			4096

extern const struct accelgyro_drv lps22hb_drv;

#endif  /* __CROS_EC_TEMP_SENSOR_LPS22HB_H */

