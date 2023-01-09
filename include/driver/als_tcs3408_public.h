/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * AMS TCS3408 light sensor driver
 */

#ifndef __CROS_EC_DRIVER_ALS_TCS3408_PUBLIC_H
#define __CROS_EC_DRIVER_ALS_TCS3408_PUBLIC_H

#include "accelgyro.h"

/* I2C Interface */
#define TCS3408_I2C_ADDR_FLAGS 0x39

/* NOTE: The higher the ATIME value in reg, the shorter the accumulation time */
#define TCS_MIN_ATIME 0xD6 /* 712 ms */
#define TCS_MAX_ATIME 0x78 /* 400 ms */
#define TCS_ATIME_GRANULARITY 256 /* 256 atime settings */
#define TCS_SATURATION_LEVEL 0xffff /* for 0 < atime < 0x70 */
#define TCS_DEFAULT_ATIME TCS_MIN_ATIME /* 712 ms */
#define TCS_CALIBRATION_ATIME TCS_MIN_ATIME
#define TCS_GAIN_UPSHIFT_ATIME TCS_MAX_ATIME

/* Number of different ranges supported for atime adjustment support */
#define TCS_MAX_ATIME_RANGES 13
#define TCS_GAIN_TABLE_MAX_LUX 12999
#define TCS_ATIME_GAIN_FACTOR 100 /* table values are 100x actual value */

#define TCS_MIN_AGAIN 0x00 /* 1x gain */
#define TCS_MAX_AGAIN 0x0B /* 2048x gain */
#define TCS_CALIBRATION_AGAIN 0x0A /* 512x gain */
#define TCS_DEFAULT_AGAIN TCS_CALIBRATION_AGAIN

#define TCS_MAX_INTEGRATION_TIME 2780 /* 2780us */
#define TCS_ATIME_DEC_STEP 5
#define TCS_ATIME_INC_STEP TCS_GAIN_UPSHIFT_ATIME

/* Min and Max sampling frequency in mHz */
#define TCS3408_LIGHT_MIN_FREQ 149
#define TCS3408_LIGHT_MAX_FREQ 1000
#if (CONFIG_EC_MAX_SENSOR_FREQ_MILLIHZ <= TCS3408_LIGHT_MAX_FREQ)
#error "EC too slow for light sensor"
#endif

/* saturation auto-adjustment */
struct tcs_saturation_t {
	/*
	 * Gain Scaling; must be value between 0 and 12
	 *      0 - 0.5x scaling
	 *      1 - 2x scaling
	 *      2 - 2x scaling
	 *      3 - 4x scaling
	 *	4 - 8x scaling
	 *	5 - 16x scaling
	 *	6 - 32x scaling
	 *	7 - 64x scaling
	 *	8 - 128x scaling
	 *	9 - 256x scaling
	 *	10 - 512x scaling
	 *	11 - 1024x scaling
	 *	12 - 2048x scaling
	 */
	uint8_t again;

	/* Acquisition Time, controlled by the ATIME register */
	uint8_t atime; /* ATIME register setting */
	uint16_t astep; /* ASTEP register setting */
};

/* tcs3408 rgb als driver data */
struct tcs3408_rgb_drv_data_t {
	uint8_t calibration_mode; /* 0 = normal run mode, 1 = calibration mode
				   */

	struct rgb_calibration_t calibration;
	struct tcs_saturation_t saturation; /* saturation adjustment */
};

extern const struct accelgyro_drv tcs3408_drv;
extern const struct accelgyro_drv tcs3408_rgb_drv;

void tcs3408_interrupt(enum gpio_signal signal);
int tcs3408_get_integration_time(int atime);

#endif /* __CROS_EC_DRIVER_ALS_TCS3408_PUBLIC_H */
