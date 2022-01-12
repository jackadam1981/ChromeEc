/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "driver/charger/isl923x.h"
#include "ec_commands.h"
#include "hooks.h"
#include "i2c.h"
#include "system.h"

#define CPRINTS(format, args...) cprints(CC_HOOK, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_HOOK, format, ## args)

static void boot_from_cutoff_init(void)
{
#define BOOT_FROM_CUTOFF_INPUT_VOLTAGE 4096

	/* b/213956630 */
	int val = (BOOT_FROM_CUTOFF_INPUT_VOLTAGE /
		   ISL9238_INPUT_VOLTAGE_REF_STEP)
		  << ISL9238_INPUT_VOLTAGE_REF_SHIFT;
	const uint32_t pwron_flag = EC_RESET_FLAG_INITIAL_PWR |
				    EC_RESET_FLAG_POWER_ON;
	const uint32_t reset_flag = system_get_reset_flags();

	if ((reset_flag & (pwron_flag | EC_RESET_FLAG_SYSJUMP)) == pwron_flag) {
		CPRINTS("\033[31m%s\033[m", __func__);
		i2c_write16(I2C_PORT_POWER, ISL923X_ADDR_FLAGS,
			    ISL9238_REG_INPUT_VOLTAGE, val);
	}
}
DECLARE_HOOK(HOOK_INIT, boot_from_cutoff_init, HOOK_PRIO_DEFAULT);
