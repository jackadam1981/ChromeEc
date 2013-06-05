/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery driver for Spring.
 */

#include "host_command.h"
#include "i2c.h"
#include "smart_battery.h"
#include "util.h"

#define PARAM_CUT_OFF 0x0010

int battery_command_cut_off(struct host_cmd_handler_args *args)
{
	int rv;
	uint8_t buf[3];

	buf[0] = SB_MANUFACTURER_ACCESS & 0xff;
	buf[1] = 0x10;
	buf[2] = 0x00;

	i2c_lock(I2C_PORT_BATTERY, 1);
	rv = i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR, buf, 3, NULL, 0,
		      I2C_XFER_SINGLE);
	rv = i2c_xfer(I2C_PORT_BATTERY, BATTERY_ADDR, buf, 3, NULL, 0,
		      I2C_XFER_SINGLE);
	i2c_lock(I2C_PORT_BATTERY, 0);

	if (rv)
		return EC_RES_ERROR;
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_BATTERY_CUT_OFF, battery_command_cut_off,
		     EC_VER_MASK(0));
