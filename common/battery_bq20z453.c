/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery driver for BQ20Z453.
 */

#include "host_command.h"
#include "smart_battery.h"

#define PARAM_CUT_OFF 0x0010

int battery_command_cut_off(struct host_cmd_handler_args *args)
{
	return sb_write(SB_MANUFACTURER_ACCESS, PARAM_CUT_OFF);
}

DECLARE_HOST_COMMAND(EC_CMD_BATTERY_CUT_OFF, battery_command_cut_off,
		     EC_VER_MASK(0));
