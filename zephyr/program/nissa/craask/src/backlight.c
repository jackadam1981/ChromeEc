/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "backlight.h"
#include "chipset.h"
#include "hooks.h"
#include "lid_switch.h"
#include "timer.h"

#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(nissa, CONFIG_NISSA_LOG_LEVEL);

static bool blight_delay_on;
static bool blight_delay_needed;

static void backlight_init(void)
{
	board_update_backlight = 1;
	blight_delay_needed = 1;
}
DECLARE_HOOK(HOOK_INIT, backlight_init, HOOK_PRIO_DEFAULT - 1);

static void update_backlight(void)
{
	LOG_INF("update backlight from board level");
	blight_delay_on = 0;
	blight_delay_needed = 0;
	enable_backlight(lid_is_open());
}
DECLARE_DEFERRED(update_backlight);

static void board_enable_backlight(void)
{
	int delay = 20;

	if (blight_delay_needed) {
		blight_delay_on = 1;
		LOG_INF("delay %ds to enable backlight after chipset on",
			delay);
		hook_call_deferred(&update_backlight_data, delay * SECOND);
	} else {
		enable_backlight(lid_is_open());
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_enable_backlight, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_enable_backlight, HOOK_PRIO_DEFAULT);

static void board_disable_backlight(void)
{
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF | CHIPSET_STATE_SUSPEND))
		blight_delay_needed = 1;

	enable_backlight(0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_disable_backlight, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_disable_backlight, HOOK_PRIO_DEFAULT);
