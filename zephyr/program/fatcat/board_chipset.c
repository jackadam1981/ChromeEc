/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Fatcat chipset-specific configuration */

#include "common.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"

void board_chipset_startup(void)
{
	/* Update the AC event during boot */
	extpower_update_host_events(gpio_get_level(GPIO_AC_PRESENT));
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);
