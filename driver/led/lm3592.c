/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI lm3592 driver.
 */

#include "i2c.h"
#include "lm3592.h"

inline int lm3592_write(uint8_t reg, uint8_t val)
{
	return i2c_write8(I2C_PORT_KBBL, LM3592_I2C_ADDR, reg, val);
}

inline int lm3592_read(uint8_t reg, int *val)
{
	return i2c_read8(I2C_PORT_KBBL, LM3592_I2C_ADDR, reg, val);
}

int lm3592_poweron(void)
{
	int ret = 0;

	ret |= lm3592_write(LM3592_REG_GP, 0x7);
	ret |= lm3592_write(LM3592_REG_BMAIN, 0x1F);

	return ret;
}

int lm3592_poweroff(void)
{
	int ret = 0;

	ret |= lm3592_write(LM3592_REG_GP, 0x5);
	ret |= lm3592_write(LM3592_REG_BMAIN, 0x00);

	return ret;
}

