/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TPS650830 PMIC driver
 */

#ifndef __CROS_EC_PMIC_TPS650830_H
#define __CROS_EC_PMIC_TPS650830_H

#include "i2c.h"
#include "console.h"

/* I2C interface */
#define TPS650830_I2C_ADDR1		(0x30 << 1)
#define TPS650830_I2C_ADDR2		(0x32 << 1)
#define TPS650830_I2C_ADDR3		(0x34 << 1)

/* TPS650830 registers */
#define TPS650830_REG_VENDORID		0x00
#define TPS650830_REG_PBCONFIG		0x14
#define TPS650830_REG_V5ADS3CNT		0x31
#define TPS650830_REG_V33ADSWCNT	0x32
#define TPS650830_REG_V18ACNT		0x34
#define TPS650830_REG_V1P2UCNT		0x36
#define TPS650830_REG_DISCHCNT1		0x3C
#define TPS650830_REG_DISCHCNT2		0x3D
#define TPS650830_REG_DISCHCNT3		0x3E
#define TPS650830_REG_DISCHCNT4		0x3F
#define TPS650830_REG_PWFAULT_MASK1	0xE5

/* TPS650830 register values */
#define TPS650830_VENDOR_ID		0x22

/*
 * Defined a new console output macro to avoid compilation error due to
 * multiple declaration the same macro in the board specific files.
 */
#define PMIC_CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/**
 *  Read register from TPS650830 PMIC.
 */
static inline int tps650830_i2c_read(const int reg, int *data_ptr)
{
	int ret;

	ret = i2c_read8(I2C_PORT_PMIC, TPS650830_I2C_ADDR, reg, data_ptr);
	if (ret)
		PMIC_CPRINTS("PMIC read fail: reg=0x%x, ret=%d", reg, ret);

	return ret;
}

/**
 *  Write register to TPS650830 PMIC.
 */
static inline int tps650830_i2c_write(const int reg, int data)
{
	int ret;

	ret = i2c_write8(I2C_PORT_PMIC, TPS650830_I2C_ADDR, reg, data);
	if (ret)
		PMIC_CPRINTS("PMIC write fail: reg=0x%x, ret=%d", reg, ret);

	return ret;
}

/**
 * Initialise TPS650830 PMIC.
 */
static inline int tps650830_init(void)
{
	int data;
	int ret;

	ret = tps650830_i2c_read(TPS650830_REG_VENDORID, &data);
	if (ret || data != TPS650830_VENDOR_ID)
		PMIC_CPRINTS("PMIC init failed: ret=%d, data=0x%x\n",
			ret, data);

	return ret;
}

#endif	/* __CROS_EC_PMIC_TPS650830_H */
