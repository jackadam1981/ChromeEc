/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI OPT3001 light sensor driver
 */

#ifndef __CROS_EC_ALS_OPT3001_H
#define __CROS_EC_ALS_OPT3001_H

/* I2C interface */
#define OPT3001_I2C_ADDR1		(0x44 << 1)
#define OPT3001_I2C_ADDR2		(0x45 << 1)
#define OPT3001_I2C_ADDR3		(0x46 << 1)
#define OPT3001_I2C_ADDR4		(0x47 << 1)

/* OPT3001 registers */
#define OPT3001_REG_RESULT		0x00
#define OPT3001_REG_CONFIGURE		0x01
#define OPT3001_REG_INT_LIMIT_LSB	0x02
#define OPT3001_REG_INT_LIMIT_MSB	0x03
#define OPT3001_REG_MAN_ID		0x7E
#define OPT3001_REG_DEV_ID		0x7F

/* OPT3001 register values */
#define OPT3001_MANUFACTURER_ID		0x5449
#define OPT3001_DEVICE_ID		0x3001

/* OPT3001 Full-Scale Range */
enum opt3001_range {
	OPT3001_RANGE_40P95_LUX,
	OPT3001_RANGE_81P90_LUX,
	OPT3001_RANGE_163P80_LUX,
	OPT3001_RANGE_327P60_LUX,
	OPT3001_RANGE_655P20_LUX,
	OPT3001_RANGE_1310P40_LUX,
	OPT3001_RANGE_2620P80_LUX,
	OPT3001_RANGE_5241P60_LUX,
	OPT3001_RANGE_10483P20_LUX,
	OPT3001_RANGE_20966P40_LUX,
	OPT3001_RANGE_41932P80_LUX,
	OPT3001_RANGE_83865P60_LUX,
	OPT3001_RANGE_AUTOMATIC_FULL_SCALE,
};

#define OPT3001_GET_DATA(_s)	((struct opt3001_drv_data_t *)(_s)->drv_data)

struct opt3001_drv_data_t {
#ifdef HAS_TASK_ALS
	int attenuation_factor;
#else
	enum opt3001_range range;
	int rate;
#endif
};

#ifdef HAS_TASK_ALS
extern const struct als_driver opt3001_drv;
#else
extern const struct accelgyro_drv opt3001_drv;
extern struct opt3001_drv_data_t g_opt3001_data;
#endif

#ifdef CONFIG_CMD_I2C_STRESS_TEST_ALS
extern struct i2c_stress_test_dev opt3001_i2c_stress_test_dev;
#endif

#endif	/* __CROS_EC_ALS_OPT3001_H */
