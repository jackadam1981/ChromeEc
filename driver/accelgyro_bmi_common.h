/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* BMI accelerometer and gyro common definitions for Chrome EC */

#ifndef __CROS_EC_ACCELGYRO_BMI_COMMON_H
#define __CROS_EC_ACCELGYRO_BMI_COMMON_H

/* odr = 100 / (1 << (8 - reg)) , within limit */
#define BMI_ODR_0_78HZ       0x01
#define BMI_ODR_100HZ        0x08

#define BMI_REG_TO_ODR(_regval) \
	((_regval) < BMI_ODR_100HZ ? 100000 / (1 << (8 - (_regval))) : \
					100000 * (1 << ((_regval) - 8)))
#define BMI_ODR_TO_REG(_odr) \
	((_odr) < 100000 ? (__builtin_clz(100000 / ((_odr) + 1)) - 24) : \
			   (39 - __builtin_clz((_odr) / 100000)))

#endif /* __CROS_EC_ACCELGYRO_BMI_COMMON_H */
