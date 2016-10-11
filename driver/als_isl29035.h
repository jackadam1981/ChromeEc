/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intersil ILS29035 light sensor driver
 */

#ifndef __CROS_EC_ALS_ISL29035_H
#define __CROS_EC_ALS_ISL29035_H

#define ILS29035_I2C_ADDR       0x88
#define ILS29035_REG_COMMAND_I  0
#define ILS29035_REG_COMMAND_II 1
#define ILS29035_REG_DATA_LSB   2
#define ILS29035_REG_DATA_MSB   3
#define ILS29035_REG_INT_LT_LSB 4
#define ILS29035_REG_INT_LT_MSB 5
#define ILS29035_REG_INT_HT_LSB 6
#define ILS29035_REG_INT_HT_MSB 7
#define ILS29035_REG_ID         15

struct isl29035_drv_data_t {
	int attenuation_factor;
};

#ifdef HAS_TASK_ALS
extern const struct als_driver isl29035_drv;
#else
extern const struct accelgyro_drv isl29035_drv;
extern struct isl29035_drv_data_t g_isl29035_data;
#endif

#endif	/* __CROS_EC_ALS_ISL29035_H */
