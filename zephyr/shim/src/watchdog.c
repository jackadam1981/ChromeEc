/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/watchdog.h>
#include <zephyr.h>

#include "config.h"
#include "hooks.h"
#include "watchdog.h"

static void wdt_warning_handler(const struct device *wdt_dev, int channel_id)
{
	/* TODO(b/176523207): watchdog warning message */
	printk("watchdog is expired\n");
}

int watchdog_init(void)
{
	int err;
	const struct device *wdt;
	struct wdt_timeout_cfg wdt_config;

	wdt = device_get_binding(DT_LABEL(DT_NODELABEL(twd0)));
	if (!wdt) {
		printk("Cannot get WDT device\n");
		return -1;
	}

	/* Unlock & stop watchdog */
	wdt_disable(wdt);

	/* Reset SoC when watchdog timer expires. */
	wdt_config.flags = WDT_FLAG_RESET_SOC;

	/*
	 * The Warning timer = CONFIG_WATCHDOG_PERIOD_MS.
	 * The watchdog reset time
	 * = CONFIG_WATCHDOG_PERIOD_MS + time of CONFIG_WDT_NPCX_DELAY_CYCLES
	 */
	wdt_config.window.min = 0U;
	wdt_config.window.max = CONFIG_WATCHDOG_PERIOD_MS;
	wdt_config.callback = wdt_warning_handler;

	err = wdt_install_timeout(wdt, &wdt_config);
	if (err < 0) {
		printk("Watchdog install error\n");
		return err;
	}

	err = wdt_setup(wdt, 0);
	if (err < 0) {
		printk("Watchdog setup error\n");
		return err;
	}

	return EC_SUCCESS;
}

void watchdog_reload(void)
{
	const struct device *wdt;

	wdt = device_get_binding(DT_LABEL(DT_NODELABEL(twd0)));
	if (!wdt) {
		printk("Cannot get WDT device\n");
		return;
	}

	wdt_feed(wdt, 0);
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);
