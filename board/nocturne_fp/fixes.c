
/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* A place to organize legacy fixes and overrides */

#include "bkpdata.h"
#include "ec_commands.h" /* Reset cause */
#include "gpio.h"
#include "system.h"
#include "hooks.h"

static void system_forge_reset_flag_por(void) {
	/* Add in preserved flag for user */
	const uint32_t flags = EC_RESET_FLAG_POWER_ON | EC_RESET_FLAG_PRESERVED;

	/* Modify current flags, in case a reset with preserve flags occurs */
	system_set_reset_flags(flags);

	/* Preserve flags in case a reset pulse occurs */
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS, flags & 0xffff);
	bkpdata_write(BKPDATA_INDEX_SAVED_RESET_FLAGS_2, flags >> 16);
}

static void board_init_fixes(void)
{
	if (!gpio_get_level(GPIO_WP)) {
		system_forge_reset_flag_por();
	}
}
DECLARE_HOOK(HOOK_INIT, board_init_fixes, HOOK_PRIO_DEFAULT - 1);