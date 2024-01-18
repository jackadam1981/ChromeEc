/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Vishay VEML3328 light sensor driver
 */

#ifndef __CROS_EC_ALS_VEML3328_H
#define __CROS_EC_ALS_VEML3328_H

/* I2C interface */
#define VEML3328_I2C_ADDR		0x10

/* Register definition */
#define VEML3328_REG_CONF		0x00
#define VEML3328_REG_C_DATA		0x04
#define VEML3328_REG_R_DATA		0x05
#define VEML3328_REG_G_DATA		0x06
#define VEML3328_REG_B_DATA		0x07
#define VEML3328_REG_IR_DATA	0x08
#define VEML3328_REG_ID			0x0C

/* Register value definition : CONF */
#define VEML3328_SD				0x8001
#define VEML3328_IT_MASK		0x0030
#define VEML3328_IT_50MS		0x0000
#define VEML3328_IT_100MS		0x0010
#define VEML3328_IT_200MS		0x0020
#define VEML3328_IT_400MS		0x0030
#define VEML3328_IT_SHIFT		4
#define VEML3328_HD_MASK		0x0040
#define VEML3328_HD_X1			0x0000
#define VEML3328_HD_X1_3		0x0040
#define VEML3328_HD_SHIFT		6
#define VEML3328_GAIN_2_MASK	0x0C00
#define VEML3328_GAIN_2_X1_2	0x0C00
#define VEML3328_GAIN_2_X1		0x0000
#define VEML3328_GAIN_2_X2		0x0400
#define VEML3328_GAIN_2_X4		0x0800
#define VEML3328_GAIN_2_SHIFT	10
#define VEML3328_GAIN_1_MASK	0x3000
#define VEML3328_GAIN_1_X1		0x0000
#define VEML3328_GAIN_1_X2		0x1000
#define VEML3328_GAIN_1_X4		0x2000
#define VEML3328_GAIN_1_SHIFT	12
#define VEML3328_SD_R_B			0x4000
#define VEML3328_CONF_DEFAULT   (VEML3328_IT_100MS | VEML3328_HD_X1 | VEML3328_GAIN_2_X1 | VEML3328_GAIN_1_X2)

/* default Itime is about 10Hz */
#define VEML3328_10000_MHZ (10 * 1000)
#define VEML3328_MAX_FREQ VEML3328_10000_MHZ
/*
 * 10Hz is too fast for the AP: allow the AP query data less often, the EC will
 * downsample.
 */
#define VEML3328_MIN_FREQ (VEML3328_MAX_FREQ / 100)

/** Default values loaded in probe function */
#define VEML3328_DEV_ID			(0x28)

typedef struct _VEML3328_CALIB {
	struct _per_model {
		// Lux
		float	LG, LC;
		float	Lh0, Lh1, Ll0, Ll1;
		float	Ch_min, Ch_max, Cl_min, Cl_max;
		float	Jh, Jl;
		// CCT
		float	Lccti0, Lccti1;
		float	X1, X2, Y1;
		// xy
		float	A0, A1, A2;
		float	B0, B1, B2;
		float	Dx_min, Dx_max;
		float	Dy_min, Dy_max;
	} per_model;
	struct _per_system {
		// Lux
		float	C_lux;
		// CCT
		float	C_ccti;
		// xy
		float	Cx0, Cx1, Cx2;
        float   Cy0, Cy1, Cy2;
	} per_system;
} VEML3328_CALIB;

extern const struct accelgyro_drv veml3328_drv;

struct veml3328_drv_data_t {
	int rate;
	int last_value;
	VEML3328_CALIB calib;
};

#endif /* __CROS_EC_ALS_VEML3328_H */
