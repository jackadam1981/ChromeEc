/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * SB-TSI: SB Temperature Sensor Interface.
 * This is an I2C slave temp sensor on the AMD Stony Ridge FT4 SOC.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "sb_tsi.h"
#include "util.h"

int sb_tsi_get_val(int idx, int *temp_ptr)
{
	/* There is only one temp sensor on the FT4 */
	if (idx != 0)
		return EC_ERROR_PARAM1;
	return EC_ERROR_NOT_POWERED;
}
