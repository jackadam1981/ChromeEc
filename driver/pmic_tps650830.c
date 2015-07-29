/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TPS650830 PMIC driver
 */

#include "console.h"
#include "driver/pmic_tps650830.h"
#include "i2c.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_I2C, format, ## args)

/**
 *  Read register from TPS650830 PMIC.
 */
int tps650830_i2c_read(const int reg, int *data_ptr)
{
	int ret;

	ret = i2c_read8(I2C_PORT_PMIC, TPS650830_I2C_ADDR, reg, data_ptr);
	if (ret)
		CPRINTS("PMIC read fail: reg=0x%x, ret=%d", reg, ret);

	return ret;
}

/**
 *  Write register to TPS650830 PMIC.
 */
int tps650830_i2c_write(const int reg, int data)
{
	int ret;

	ret = i2c_write8(I2C_PORT_PMIC, TPS650830_I2C_ADDR, reg, data);
	if (ret)
		CPRINTS("PMIC write fail: reg=0x%x, ret=%d", reg, ret);

	return ret;
}

/**
 * Initialise TPS650830 PMIC.
 */
int tps650830_init(void)
{
	int data;
	int ret;

	ret = tps650830_i2c_read(TPS650830_REG_VENDORID, &data);
	if (ret || data != TPS650830_VENDOR_ID)
		CPRINTS("PMIC init failed: ret=%d, data=0x%x\n", ret, data);

	return ret;
}
