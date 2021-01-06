/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <device.h>
#include <drivers/watchdog.h>
#include <zephyr.h>

#include "system.h"

void system_watchdog_reset(void)
{
	int err;
	const struct device *wdt;
	struct wdt_timeout_cfg wdt_config;

	wdt = device_get_binding(DT_LABEL(DT_NODELABEL(twd0)));
	if (!wdt) {
		printk("Cannot get WDT device\n");
		return;
	}

	/* Unlock & stop watchdog */
	wdt_disable(wdt);

	/* Reset SoC when watchdog timer expires. */
	wdt_config.flags = WDT_FLAG_RESET_SOC;

	/*
	 * Set the watchdog reset time to mimnum value.
	 * Current reset time
	 * = LFCG period * TWCP * WDCP * WDCNT
	 * = (1 / 32768) * 32 * 32 * (1 + CONFIG_WDT_NPCX_DELAY_CYCLES)
	 * ~= 0.53s
	 */
	wdt_config.window.min = 0U;
	wdt_config.window.max = 1U;
	wdt_config.callback = NULL;

	err = wdt_install_timeout(wdt, &wdt_config);
	if (err < 0) {
		printk("Watchdog install error: %d\n", -err);
		return;
	}

	err = wdt_setup(wdt, 0);
	if (err < 0) {
		printk("Watchdog setup error: %d\n", -err);
		return;
	}

	while (1) {
		continue;
	}
}

void system_reset(int flags)
{
	uint32_t save_flags;

	/* Disable interrupts to avoid task swaps during reboot */
	irq_lock();

	/*  Get the flags to be saved */
	system_encode_save_flags(flags, &save_flags);

	/* TODO(b/176523207): Store the reset flags. */

	/* If WAIT_EXT is set, then allow 10 seconds for external reset */
	if (flags & SYSTEM_RESET_WAIT_EXT) {
		int i;

		/* Wait 10 seconds for external reset */
		for (i = 0; i < 1000; i++) {
			watchdog_reload();
			k_busy_wait(10 * USEC_PER_MSEC);
		}
	}

	/* trigger a reboot */
	system_watchdog_reset();

	/* Spin and wait for reboot; should never return */
	while (1)
		;
}
