/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * BMP280 pressure and temperature module for Chrome EC
 */

#ifndef __CROS_EC_BARO_BMP280_H
#define __CROS_EC_BARO_BMP280_H

#include <third_party/bmp280/bmp280.h>

/*
 * The addr field of barometer_sensor for I2C:
 *
 * +-------------------------------+---+
 * |    7 bit i2c address          | 0 |
 * +-------------------------------+---+
 */

/*
 * Bit 1 of 7-bit address: 0 - If SDO is connected to GND
 * Bit 1 of 7-bit address: 1 - If SDO is connected to Vddio
 */
#define BMP280_I2C_ADDRESS1_FLAGS	0x76
#define BMP280_I2C_ADDRESS2_FLAGS	0x77

/* CHIP ID */
#define BMP280_CHIP_ID			0x58

/* SAMPLING PERIOD COMPUTATION CONSTANT */
#define BMP280_STANDBY_CNT		8

/*
 * This is the measurement time required for pressure and temp
 */
#define BMP280_COMPUTE_TIME \
	((T_INIT_MAX + T_MEASURE_PER_OSRS_MAX * \
	  ((BIT(BMP280_OVERSAMP_TEMP) >> 1) + \
	   (BIT(BMP280_OVERSAMP_PRES) >> 1)) + \
	  (BMP280_OVERSAMP_PRES ? T_SETUP_PRESSURE_MAX : 0) + 15) / 16)

/*
 * These values are selected as per Bosch recommendation for
 * standard handheld devices, with temp sensor not being used
 */
#define BMP280_OVERSAMP_PRES BMP280_OVERSAMP_4X
#define BMP280_OVERSAMP_TEMP BMP280_OVERSAMP_SKIPPED
/*******************************************************/
/*             GET DRIVER DATA			       */
/*******************************************************/
#define BMP280_GET_DATA(_s) \
	((struct bmp280_drv_data_t *)(_s)->drv_data)

/* Min and Max sampling frequency in mHz based on x4 oversampling used */
/* FIXME - verify how chip is setup to make sure MAX is correct, manual says
 * "Typical", not Max.
 */
#define BMP280_BARO_MIN_FREQ  75000
#define BMP280_BARO_MAX_FREQ  87000
#if (CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ <= BMP280_BARO_MAX_FREQ)
#error "EC too slow for accelerometer"
#endif

/*
 * struct bmp280_t - This structure holds BMP280 initialization parameters
 * @calib_param:          calibration data
 * @rate:     frequency, in mHz.
 * @range:		bit offset to fit data in 16 bit or less.
 */
struct bmp280_drv_data_t {

	struct   bmp280_calib_param_t calib_param;
	uint16_t rate;
	uint16_t range;
};
#define BMP280_RATE_SHIFT 1

extern const struct accelgyro_drv bmp280_drv;

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ACCEL
extern struct i2c_stress_test_dev bmp280_i2c_stress_test_dev;
#endif

#endif
