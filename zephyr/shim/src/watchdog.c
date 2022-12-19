/* Copyright 2021 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "config.h"
#include "hooks.h"
#include "watchdog.h"

#include <zephyr/device.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(watchdog_shim, LOG_LEVEL_ERR);

struct watchdog_info {
	const struct device *wdt_dev;
	struct wdt_timeout_cfg config;
};

static void wdt_warning_handler(const struct device *wdt_dev, int channel_id);

const struct watchdog_info wdt_info[] = {
	{
		.wdt_dev = DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog)),
		.config = {
#if DT_NODE_HAS_COMPAT(DT_CHOSEN(cros_ec_watchdog), st_stm32_watchdog)
			.flags = WDT_FLAG_RESET_SOC,
			.window.min = 0U,
			.window.max = CONFIG_WATCHDOG_PERIOD_MS,
			.callback = NULL,
#else
			.flags = WDT_FLAG_RESET_SOC,
			.window.min = 0U,
			.window.max = CONFIG_AUX_TIMER_PERIOD_MS,
			.callback = wdt_warning_handler,
#endif
		},
	},
#ifdef CONFIG_PLATFORM_EC_WATCHDOG_HELPER
	{
		.wdt_dev = DEVICE_DT_GET(DT_CHOSEN(cros_ec_watchdog_helper)),
		.config = {
			.flags = 0U,
			.window.min = 0U,
			.window.max = CONFIG_AUX_TIMER_PERIOD_MS,
			.callback = wdt_warning_handler,
		},
	},
#endif
};

/* Array to keep channel used to implement watchdog */
int wdt_chan[ARRAY_SIZE(wdt_info)];

/* Array to keep information if watchdog is enabled */
bool wdt_enabled[ARRAY_SIZE(wdt_info)];

#ifdef TEST_BUILD
extern bool wdt_warning_triggered;
#endif /* TEST_BUILD */

static void wdt_warning_handler(const struct device *wdt_dev, int channel_id)
{
	const char *thread_name = k_thread_name_get(k_current_get());

#ifdef CONFIG_RISCV
	printk("WDT pre-warning MEPC:%p THREAD_NAME:%s\n",
	       (void *)csr_read(mepc), thread_name);
#else
	/* TODO(b/176523207): watchdog warning message */
	printk("Watchdog deadline is close! THREAD_NAME:%s\n", thread_name);
#endif
#ifdef TEST_BUILD
	wdt_warning_triggered = true;
#endif
#ifdef CONFIG_SOC_SERIES_MEC172X
	extern void cros_chip_wdt_handler(const struct device *wdt_dev,
					  int channel_id);
	cros_chip_wdt_handler(wdt_dev, channel_id);
#endif

	/*
	 * Watchdog is disabled after calling handler. Mark it as disabled so we
	 * can re-enable it on next reload.
	 */
	for (int i = 0; i < ARRAY_SIZE(wdt_info); i++) {
		if (wdt_info[i].wdt_dev == wdt_dev) {
			wdt_enabled[i] = false;
			break;
		}
	}
}

static int watchdog_config(const struct device *wdt_dev,
			   const struct wdt_timeout_cfg *config)
{
	int chan;

	if (!device_is_ready(wdt_dev)) {
		LOG_ERR("Error: device %s is not ready", wdt_dev->name);
		return -1;
	}

	chan = wdt_install_timeout(wdt_dev, config);

	/* If watchdog is running, reinstall it. */
	if (chan == -EBUSY) {
		wdt_disable(wdt_dev);
		chan = wdt_install_timeout(wdt_dev, config);
	}

	if (chan < 0) {
		LOG_ERR("Watchdog install error: %d", chan);
		return chan;
	}

	return chan;
}

int watchdog_init(void)
{
	int err = EC_SUCCESS;

	for (int i = 0; i < ARRAY_SIZE(wdt_info); i++) {
		wdt_enabled[i] = false;
		wdt_chan[i] = watchdog_config(wdt_info[i].wdt_dev,
					      &wdt_info[i].config);

		if (wdt_chan[i] < 0) {
			err = wdt_chan[i];
		}
	}

	/* Start watchdog */
	watchdog_reload();

	return err;
}

void watchdog_reload(void)
{
	int err;

	for (int i = 0; i < ARRAY_SIZE(wdt_info); i++) {
		if (!device_is_ready(wdt_info[i].wdt_dev)) {
			LOG_ERR("Error: device %s is not ready",
				wdt_info[i].wdt_dev->name);
			continue;
		}

		if (wdt_chan[i] < 0)
			continue;

		if (!wdt_enabled[i]) {
			err = wdt_setup(wdt_info[i].wdt_dev, 0);
			if (err < 0) {
				LOG_ERR("Watchdog setup error: %d", err);
				continue;
			}

			wdt_enabled[i] = true;
		}

		wdt_feed(wdt_info[i].wdt_dev, wdt_chan[i]);
	}
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);
