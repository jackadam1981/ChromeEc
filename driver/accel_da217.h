/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* MIR3DA gsensor module for Chrome EC */

#ifndef __CROS_EC_ACCEL_MIR3DA_H
#define __CROS_EC_ACCEL_MIR3DA_H


struct mir3da_accel_data {
	struct accelgyro_saved_data_t base;
	/* Current resolution of accelerometer. */
	int sensor_resolution;
	int16_t offset[3];
};

/******************************************************************/
#define MIR3DA_ADDR1_FLAGS             0x26
#define MIR3DA_ADDR2_FLAGS             0x27

#define MIR3DA_RESOLUTION			   0x00
#define MIR3DA_RESOLUTION_RANGE        0x0F
#define MIR3DA_RANGE_MSK               0x03
#define MIR3DA_RANGE_2G                0
#define MIR3DA_RANGE_4G                1
#define MIR3DA_RANGE_8G                2
#define MIR3DA_RANGE_16G               3

#define MIR3DA_RANGE_TO_REG(_range) \
	((_range) < 8 ? MIR3DA_RANGE_2G + ((_range) / 4) * 2 : \
			MIR3DA_RANGE_8G + ((_range) / 16) * 4)

#define MIR3DA_REG_TO_RANGE(_reg) \
	((_reg) < MIR3DA_RANGE_8G ? 2 + (_reg) - MIR3DA_RANGE_2G : \
				    8 + ((_reg) - MIR3DA_RANGE_8G) * 2)

#define MIR3DA_ODR_AXIS                  0x10
#define MIR3DA_ODR_MSK                   0x0F
#define MIR3DA_ODR_7_81HZ                0x04 /* ODR  15.625HZ */
#define MIR3DA_ODR_15_63HZ               0x05 /* ODR  31.25HZ */
#define MIR3DA_ODR_31_25HZ               0x06 /* ODR  62.50HZ */
#define MIR3DA_ODR_62_50HZ               0x07 /* ODR  125HZ */
#define MIR3DA_ODR_125HZ                 0x08 /* ODR  250HZ */
#define MIR3DA_ODR_250HZ                 0x09 /* ODR  500HZ */
#define MIR3DA_ODR_500HZ                 0x0A /* ODR  1000HZ */

#define REG_CHIP_ID              0x01
#define MIR3DA_CHIPID            0x13
#define MIR3DA_RESET_VALUE       0x24
#define MIR3DA_POWER_ON          0x30
#define MIR3DA_POWER_OFF         0x9E

#define MIR3DA_SPI_CONFIG        0x00
#define MIR3DA_ACC_X_LSB         0x02
#define MIR3DA_MODE_BW           0x11

#define MIR3DA_ODR_TO_REG(_bw) \
	((_bw) < 125000 ? \
	 MIR3DA_ODR_7_81HZ + __fls(((_bw) * 10) / 78125) : \
	 MIR3DA_ODR_125HZ + __fls((_bw) / 125000))

#define  MIR3DA_REG_TO_BW(_reg) \
	((_reg) < MIR3DA_ODR_125HZ ? \
	 (78125 << ((_reg) - MIR3DA_ODR_7_81HZ)) / 10 : \
	 125000 << ((_reg) - MIR3DA_ODR_125HZ))

extern const struct accelgyro_drv da217_accel_drv;

#endif /* __CROS_EC_ACCEL_MIR3DA_H */
