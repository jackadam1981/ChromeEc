/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * CAPELLA CM32181E light sensor driver
 */

#ifndef __CROS_EC_ALS_CM32181E_H
#define __CROS_EC_ALS_CM32181E_H

/* I2C interface */
/* Address Select Pin pull low */
#define CM32181E_I2C_ADDR1		(0x10 << 1)
/* Address Select Pin pull high */
#define CM32181E_I2C_ADDR2		(0x48 << 1)

/* CM32181E registers */
#define CM32181E_REG_RESULT		0x04
#define CM32181E_REG_CONFIGURE		0x00

/* Min and Max sampling frequency in mHz
 * initial as 0x00C0, refresh time is 1300ms, about 7000mhz
 */
#define CM32181E_LIGHT_MIN_FREQ			20
#define CM32181E_LIGHT_MAX_FREQ			7000

struct cm32181e_drv_data {
	int rate;
	int last_value;
	/* the coefficient is scale.uscale */
	int16_t scale;
	uint16_t uscale;
	int16_t offset;
};

extern const struct accelgyro_drv cm32181e_drv;
#endif	/* __CROS_EC_ALS_CM32181E_H */
