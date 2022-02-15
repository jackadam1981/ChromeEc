/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Herobrine chipset-specific configuration */

#include "common.h"
#include "gpio.h"
#include "hooks.h"

/* Called on AP S5 -> S3 transition */
static void board_chipset_pre_init(void)
{
	static bool pp5000_inited;

	if (!pp5000_inited) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), 1);
		pp5000_inited = true;
	}
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);
