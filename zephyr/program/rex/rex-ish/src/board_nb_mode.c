/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"
#include "tablet_mode.h"

static void board_nb_mode_change(void)
{
	/*
	 * gpio_ec_nb_mode_l is an active low pin; default level is low.
	 * This pin is an output going to EC.
	 * When system is in notebook(clamshell) mode, set pin level to low.
	 * When system is in tablet mode, set pin level to high.
	 * ISH updates this gpio to notify EC about mode change.
	 */
	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_ec_nb_mode_l),
			tablet_get_mode());
}
DECLARE_HOOK(HOOK_TABLET_MODE_CHANGE, board_nb_mode_change, HOOK_PRIO_DEFAULT);
