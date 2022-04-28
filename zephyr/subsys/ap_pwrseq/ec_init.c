/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <init.h>
#include <ap_power/ap_power_interface.h>

#include "ec_commands.h"
#include "system.h"

/*
 * Initialize power sequence initialisation flags.
 */
static int ec_init_power_flags(const struct device *dev)
{
	enum ap_power_init_flags flags = 0;
	uint32_t reset_flags = system_get_reset_flags();

	/*
	 * Copy over the system reset flags to the init flags.
	 */
	if (reset_flags & EC_RESET_FLAG_AP_OFF) {
		flags |= AP_POWER_INIT_OFF;
	}
	if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		flags |= AP_POWER_INIT_WARM_START;
	}

	ap_power_set_init_flags(flags);
	return 0;
}
/*
 * The init flag setting must occur before the power sequence init is done.
 */
SYS_INIT(ec_init_power_flags, POST_KERNEL, 50);
