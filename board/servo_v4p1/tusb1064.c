/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "tusb1064.h"
#include "ioexpanders.h"

int init_tusb1064(int port)
{
	uint8_t tmp;

	/* Enable the TUSB1064 redriver */
	cmux_en(1);

	/* Disconnect USB3.1 and DP */
	tmp = tusb1064_read_byte(port, TUSB1064_REG_GENERAL);
	if (tmp < 0)
		return EC_ERROR_INVAL;

	tmp &= ~REG_GENERAL_CTLSEL_2_DP_AND_USB3_1;
	if (tusb1064_write_byte(port, TUSB1064_REG_GENERAL, tmp))
		return EC_ERROR_INVAL;

	return EC_SUCCESS;
}

int tusb1064_write_byte(int port, uint8_t reg, uint8_t val)
{
	return i2c_write8(port, TUSB1064_ADDR_FLAGS, reg, val);
}

int tusb1064_read_byte(int port, uint8_t reg)
{
	int tmp;

	if (i2c_read8(port, TUSB1064_ADDR_FLAGS, reg, &tmp))
		return -1;

	return tmp;
}
