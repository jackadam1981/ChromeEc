/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "power.h"
#include "system.h"

/*
 * The only use for chipset state is sensors, so we hard code the AP state to on
 * and make the sensor on in S0. The sensors are always on when the ISH is
 * powered.
 */
int chipset_in_state(int state_mask)
{
	return state_mask & CHIPSET_STATE_ON;
}

/*TODO: Work on those with power management */
void chip_save_reset_flags(uint32_t flags)
{
}

uint32_t chip_read_reset_flags(void)
{
	return EC_RESET_FLAG_POWER_ON;
}

test_export_static int system_preinitialize(const struct device *unused)
{
	ARG_UNUSED(unused);

	system_set_reset_flags(chip_read_reset_flags());
	return 0;
}

SYS_INIT(system_preinitialize, PRE_KERNEL_1,
	 CONFIG_PLATFORM_EC_SYSTEM_PRE_INIT_PRIORITY);
