/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * MiraMEMS Accelerometer driver for Chrome EC
 *
 * Supported: DA217
 */

#ifndef __CROS_EC_ACCEL_DA217_H
#define __CROS_EC_ACCEL_DA217_H

#include "accelgyro.h"

struct da217_accel_data {
	struct accelgyro_saved_data_t base;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

/******************************************************************/
#define DA217_ADDR1_FLAGS             0x26
#define DA217_ADDR2_FLAGS             0x27

/* Min and Max sampling frequency in mHz */
#define DA217_ACCEL_MIN_FREQ	12500
#define DA217_ACCEL_MAX_FREQ	MOTION_MAX_SENSOR_FREQUENCY(1600000, 6250)

#define DA217_14_BIT_RESOLUTION       14
#define DA217_REG_RESOLUTION_RANGE    0x0F
#define DA217_RANGE_MASK              GENMASK(1, 0)
#define DA217_RANGE_2G                0
#define DA217_RANGE_4G                1
#define DA217_RANGE_8G                2
#define DA217_RANGE_16G               3

#define DA217_RANGE_TO_REG(_range) \
	((_range) < 8 ? DA217_RANGE_2G + ((_range) / 4) : \
			DA217_RANGE_8G + ((_range) / 16))

#define DA217_REG_TO_RANGE(_reg) \
	((_reg) < DA217_RANGE_8G ? 2 + ((_reg) - DA217_RANGE_2G) * 2 : \
			8 + ((_reg) - DA217_RANGE_8G) * 8)

#define DA217_REG_ODR_AXIS              0x10
#define DA217_ODR_MASK                  GENMASK(3, 0)
#define DA217_ODR_7_81HZ                0x03 /* ODR  7.81HZ */
#define DA217_ODR_15_63HZ               0x04 /* ODR  15.63HZ */
#define DA217_ODR_31_25HZ               0x05 /* ODR  31.25HZ */
#define DA217_ODR_62_50HZ               0x06 /* ODR  62.50HZ */
#define DA217_ODR_125HZ                 0x07 /* ODR  123HZ */
#define DA217_ODR_250HZ                 0x08 /* ODR  250HZ */
#define DA217_ODR_500HZ                 0x09 /* ODR  500HZ */

#define DA217_REG_CHIP_ID       0x01
#define DA217_CHIPID            0x13

#define DA217_REG_SPI_CONFIG    0x00
#define DA217_ACC_X_LSB         0x02
#define DA217_RESET_VALUE       0x24

#define DA217_REG_MODE_BW       0x11
#define DA217_POWER_ON_VALUE    0x30
#define DA217_POWER_OFF_VALUE   0x9E

#define DA217_RATE_TO_REG(_rate) \
	((_rate) < 125000 ? \
	 DA217_ODR_7_81HZ + __fls(((_rate) * 10) / 78125) : \
	 DA217_ODR_125HZ + __fls((_rate) / 125000))

#define  DA217_REG_TO_RATE(_reg) \
	((_reg) < DA217_ODR_125HZ ? \
	 (78125 << ((_reg) - DA217_ODR_7_81HZ)) / 10 : \
	 125000 << ((_reg) - DA217_ODR_125HZ))

extern const struct accelgyro_drv da217_accel_drv;

#endif /* __CROS_EC_ACCEL_DA217_H */
