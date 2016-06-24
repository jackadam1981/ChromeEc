/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Dyna-Image AL3010 light sensor driver
 */

#ifndef __CROS_EC_ALS_AL3010_H
#define __CROS_EC_ALS_AL3010_H

/* I2C interface */
#define AL3010_I2C_ADDR1		(0x1C << 1)
#define AL3010_I2C_ADDR2		(0x1D << 1)
#define AL3010_I2C_ADDR3		(0x1E << 1)

/* AL3010 registers */
#define AL3010_REG_SYSTEM		0x00
#define AL3010_REG_CONFIG		0x10
#define AL3010_REG_DATA_LOW		0x0C

enum al3010_range {
	AL3010_RANGE_1, /* 77806 lx */
	AL3010_RANGE_2, /* 19452 lx  */
	AL3010_RANGE_3, /* 4863  lx  */
	AL3010_RANGE_4  /* 1216  lx  */
};

#define AL3010_RANGE	AL3010_RANGE_3
#define AL3010_ENABLE	0x01

int al3010_init(void);
int al3010_read_lux(int *lux, int af);

#endif	/* __CROS_EC_ALS_AL3010_H */
