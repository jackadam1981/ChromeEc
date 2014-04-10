/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pure hardware thermal shutdown via TMP432 for Quawks.
 */

#include "board.h"
#include "console.h"
#include "hooks.h"
#include "tmp432.h"
#include "i2c.h"

#define CPRINTF(format, args...) cprintf(CC_THERMAL, format, ## args)

/**
 * Set pure hardware thermal shutdown temperature via TMP432.
 *
 * The target temperature value is set in board.h.
 */
static void set_pure_hardware_thermal_shutdown_temp(void)
{
	int value;

	if ( i2c_write8(I2C_PORT_THERMAL, TMP432_I2C_ADDR, TMP432_LOCAL_HIGH_LIMIT_W, CONFIG_THERMAL_SHUTDOWN_TEMP) >= 0 &&
		 i2c_read8( I2C_PORT_THERMAL, TMP432_I2C_ADDR, TMP432_LOCAL_HIGH_LIMIT_R, &value) >= 0)
	{
		CPRINTF("[%T] Thremal Shutdown temp now = %d\n", value);
	}
	else
	{
		CPRINTF("[%T] Failed to set shutdown temp!!\n");
	}
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, set_pure_hardware_thermal_shutdown_temp, HOOK_PRIO_DEFAULT);
