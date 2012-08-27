/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Smart battery driver for BQ20Z453.
 */

#include "host_command.h"
#include "smart_battery.h"
#include "timer.h"

#define PARAM_CUT_OFF 0x0010

int battery_command_cut_off(struct host_cmd_handler_args *args)
{
	/*
	 * TODO: Since this is a host command, the i2c bus is claimed by host.
	 *       Thus, we would send back the response in advanced so that
	 *       the host can release the bus. Then, EC can send command to
	 *       battery.
	 *
	 *       Refactoring this via task is a way. However, it is wasteful.
	 *       Need a light-weight solution.
	 */
	args->result = EC_RES_SUCCESS;
	host_send_response(args);

	/* busy try until the system is off. */
	while (1) {
		usleep(1000);  /* wait for 1ms */
		sb_write(SB_MANUFACTURER_ACCESS, PARAM_CUT_OFF);
	}

	/* Should never be here. */
	return EC_RES_ERROR;
}
DECLARE_HOST_COMMAND(EC_CMD_BATTERY_CUT_OFF, battery_command_cut_off,
		     EC_VER_MASK(0));
