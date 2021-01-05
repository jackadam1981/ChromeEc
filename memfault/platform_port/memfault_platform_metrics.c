/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#include "memfault/core/debug_log.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/metrics/metrics.h"
#include "memfault/metrics/platform/timer.h"

#include "battery.h"
#include "hooks.h"
#include "timer.h"

static MemfaultPlatformTimerCallback *memfault_timer_callback = NULL;
static int memfault_timer_period_us = 0;
static void memfault_timer_tick(void);
DECLARE_DEFERRED(memfault_timer_tick);
static void memfault_timer_tick(void) {
	MEMFAULT_LOG_INFO("memfault_timer_tick");
	if (memfault_timer_callback) {
		memfault_timer_callback();
	}
	else {
		MEMFAULT_LOG_ERROR("Memfault timer callback is not initilized");
	}
	hook_call_deferred(&memfault_timer_tick_data, memfault_timer_period_us);
}


bool memfault_platform_metrics_timer_boot(uint32_t period_sec,
					MemfaultPlatformTimerCallback callback) {
	MEMFAULT_LOG_INFO("memfault_platform_metrics_timer_boot  heartbeat_period=%d", period_sec);
	memfault_timer_callback = callback;
	memfault_timer_period_us = period_sec * SECOND;
	if (hook_call_deferred(&memfault_timer_tick_data, memfault_timer_period_us)) {
		MEMFAULT_LOG_ERROR("Error setting up memfault timer");
		return false;
	}
	return true;
}

void memfault_save_battery_level(void) {
	int charge;
	battery_state_of_charge_abs(&charge);
	MEMFAULT_LOG_INFO("memfault_save_battery_level %d",charge);
	memfault_metrics_heartbeat_set_unsigned(MEMFAULT_METRICS_KEY(battery_level), charge);
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, memfault_save_battery_level,
	     HOOK_PRIO_DEFAULT);

int memfault_platform_metrics_init(const sMemfaultEventStorageImpl *evt_storage) {

	MEMFAULT_LOG_INFO("Initializing memfault metrics info");

	MEMFAULT_LOG_INFO("memfault_reboot_tracking_get_crash_count=%d", memfault_reboot_tracking_get_crash_count());

	const sMemfaultMetricBootInfo boot_info = {
		.unexpected_reboot_count = memfault_reboot_tracking_get_crash_count(),
	};

	int rv = memfault_metrics_boot(evt_storage, &boot_info);

	memfault_metrics_heartbeat_debug_trigger();

	return rv;
}
