/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * SB-TSI: SB Temperature Sensor Interface.
 * This is an I2C slave temp sensor on the AMD Stony Ridge FT4 SOC.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "sb_tsi.h"
#include "util.h"

static int temp_val;

static int raw_read8(const int offset, int *data_ptr)
{
	return i2c_read8(I2C_PORT_THERMAL, SB_TSI_I2C_ADDR, offset, data_ptr);
}

static void sb_tsi_sensor_poll(void)
{
	raw_read8(SB_TSI_TEMP_H, &temp_val);
	temp_val = C_TO_K(temp_val);
}
DECLARE_HOOK(HOOK_SECOND, sb_tsi_sensor_poll, HOOK_PRIO_TEMP_SENSOR);

int sb_tsi_get_val(int idx, int *temp_ptr)
{
	if (idx != 0)
		return EC_ERROR_PARAM1;
	*temp_ptr = temp_val;
	return EC_SUCCESS;
}
