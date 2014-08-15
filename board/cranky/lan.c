/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * LAN power control for Cranky
 */

#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "util.h"

static void lan_power_enable(void)
{
	gpio_set_level(GPIO_LAN_POWER, 1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, lan_power_enable, HOOK_PRIO_DEFAULT);


static void lan_power_disable(void)
{
	gpio_set_level(GPIO_LAN_POWER, 0);
}
DECLARE_HOOK(HOOK_INIT, lan_power_disable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, lan_power_disable, HOOK_PRIO_DEFAULT);
