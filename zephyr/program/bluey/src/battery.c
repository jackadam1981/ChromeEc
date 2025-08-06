/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Boot Mode detection and off-mode charging boot functions
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "charge_state.h"
#include "common.h"
#include "hooks.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

int cached_charge_percentage = 100;

void board_chipset_pre_init(void)
{
	if (!battery_state_of_charge_abs(&cached_charge_percentage))
		CPRINTS("Fetching battery charge state failed");

	CPRINTS("Current battery charge = %d", cached_charge_percentage);

	/*
	 * TODO: move i2c from controller mode to peripheral mode and disable
	 * battery commands
	 */
	CPRINTS("I2C_PORT_ADSP switched to peripheral mode");
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);

void board_chipset_shutdown_complete(void)
{
	/*
	 * TODO: move i2c from peripheral mode to controller mode and enable
	 * battery commands
	 */
	CPRINTS("I2C_PORT_ADSP switched to controller mode");
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN_COMPLETE, board_chipset_shutdown_complete,
	     HOOK_PRIO_DEFAULT);
