/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault core interface implementation
 */


#include "memfault/core/build_info.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/platform/core.h"
#include "memfault/core/reboot_tracking.h"
#include "memfault/core/trace_event.h"

#include "memfault_platform_port.h"
#include "console.h"
#include "hooks.h"
#include "printf.h"
#include "system.h"
#include "task.h"

#define MEMFAULT_RAM_BACKED_EVENTS_SIZE 512
MEMFAULT_PUT_IN_SECTION(".noinit.mflt_events") MEMFAULT_ALIGNED(8)
static uint8_t s_ram_backed_events_region[MEMFAULT_RAM_BACKED_EVENTS_SIZE];

#ifdef CONFIG_MEMFAULT_SAVE_LOG
static uint8_t log_region[MEMFAULT_RAM_BACKED_LOG_SIZE];
#endif

MEMFAULT_PUT_IN_SECTION(".noinit.mflt_reboots") MEMFAULT_ALIGNED(8)
static uint8_t s_ram_backed_reboots_region[MEMFAULT_REBOOT_TRACKING_REGION_SIZE];

MEMFAULT_NORETURN void memfault_platform_reboot(void) {
	MEMFAULT_LOG_INFO("memfault_platform_reboot");
	system_reset(0);
	MEMFAULT_UNREACHABLE;
}

uint64_t memfault_platform_get_time_since_boot_ms(void) {
	return get_time().val / MSEC;
}

static eMemfaultRebootReason reset_flags_to_memfault_reboot_reason(uint32_t reset_flags) {
	/* TODO: Need to verify this mapping */
	if (reset_flags & EC_RESET_FLAG_OTHER) {
		return kMfltRebootReason_Unknown;
	} else if (reset_flags & EC_RESET_FLAG_RESET_PIN) {
		return kMfltRebootReason_PinReset;
	} else if (reset_flags & EC_RESET_FLAG_BROWNOUT) {
		return kMfltRebootReason_BrownOutReset;
	} else if (reset_flags & EC_RESET_FLAG_POWER_ON) {
		return kMfltRebootReason_PowerOnReset;
	} else if (reset_flags & EC_RESET_FLAG_WATCHDOG) {
		return kMfltRebootReason_HardwareWatchdog;
	} else if (reset_flags & EC_RESET_FLAG_SOFT) {
		return kMfltRebootReason_SoftwareReset;
	} else if (reset_flags & EC_RESET_FLAG_HIBERNATE) {
		return kMfltRebootReason_DeepSleep;
	} else if (reset_flags & EC_RESET_FLAG_RTC_ALARM) {
		return kMfltRebootReason_Unknown;
	} else if (reset_flags & EC_RESET_FLAG_WAKE_PIN) {
		return kMfltRebootReason_DeepSleep;
	} else if (reset_flags & EC_RESET_FLAG_LOW_BATTERY) {
		return kMfltRebootReason_LowPower;
	} else if (reset_flags & EC_RESET_FLAG_SYSJUMP) {
		return kMfltRebootReason_Unknown;
	} else if (reset_flags & EC_RESET_FLAG_HARD) {
		return kMfltRebootReason_UserShutdown;
	} else {
		return kMfltRebootReason_Unknown;
	}
}

static int memfault_platform_init(void) {
	MEMFAULT_LOG_INFO("Initializing Memfault");

	const sMemfaultEventStorageImpl *evt_storage;
	MEASURE_TIME(evt_storage=memfault_events_storage_boot(s_ram_backed_events_region, sizeof(s_ram_backed_events_region)), "memfault_events_storage_boot");
	MEASURE_TIME(memfault_trace_event_boot(evt_storage), "memfault_trace_event_boot");

	uint32_t reset_flags = system_get_reset_flags();
	sResetBootupInfo reset_reason = {
	    .reset_reason_reg = reset_flags,
	    .reset_reason = reset_flags_to_memfault_reboot_reason(reset_flags),
	};
	MEASURE_TIME(memfault_reboot_tracking_boot(s_ram_backed_reboots_region, &reset_reason), "memfault_reboot_tracking_boot");

	MEASURE_TIME(memfault_reboot_tracking_collect_reset_info(evt_storage), "memfault_reboot_tracking_collect_reset_info");

#ifdef CONFIG_MEMFAULT_SAVE_LOG
	memfault_log_boot(log_region, sizeof(log_region));
#endif

#ifdef CONFIG_MEMFAULT_METRICS
	memfault_platform_metrics_init(evt_storage);
#endif

#ifdef CONFIG_MEMFAULT_PANICS
	memfault_platform_coredump_init();
#endif

	MEMFAULT_LOG_INFO("memfault_reboot_tracking_compute_worst_case_storage_size=%d", memfault_reboot_tracking_compute_worst_case_storage_size());
	MEMFAULT_LOG_INFO("memfault_trace_event_compute_worst_case_storage_size=%d", memfault_trace_event_compute_worst_case_storage_size());

	MEMFAULT_LOG_INFO("Memfault Initialized");

	return 0;
}

static void memfault_init_hook(void) {
	memfault_platform_init();
}
DECLARE_HOOK(HOOK_INIT, memfault_init_hook, HOOK_PRIO_DEFAULT);
