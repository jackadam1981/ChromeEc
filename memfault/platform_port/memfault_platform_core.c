/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Memfault core interface implementation
 */


#include "memfault/core/platform/core.h"
#include "memfault/core/debug_log.h"
#include "memfault/core/event_storage.h"
#include "memfault/core/trace_event.h"
#include "memfault/core/reboot_tracking.h"
#include "system.h"
#include "console.h"
#include "printf.h"
#include "hooks.h"

#define MEMFAULT_RAM_BACKED_EVENTS_SIZE 256

MEMFAULT_PUT_IN_SECTION(".noinit.mflt_events") MEMFAULT_ALIGNED(8)
static uint8_t s_ram_backed_events_region[MEMFAULT_RAM_BACKED_EVENTS_SIZE];


MEMFAULT_NORETURN void memfault_platform_reboot(void) {
  MEMFAULT_LOG_INFO("memfault_platform_reboot");
  system_reset(0);
  MEMFAULT_UNREACHABLE;
}

int memfault_platform_boot(void) {
  MEMFAULT_LOG_INFO("Initializing Memfault");

  const sMemfaultEventStorageImpl *evt_storage =
      memfault_events_storage_boot(s_ram_backed_events_region, sizeof(s_ram_backed_events_region));

  // Minimum storage we need to hold at least 1 reboot event and 1 trace event
  const size_t bytes_needed = memfault_reboot_tracking_compute_worst_case_storage_size() +
      memfault_trace_event_compute_worst_case_storage_size();
  if (bytes_needed > sizeof(s_ram_backed_events_region)) {
    MEMFAULT_LOG_ERROR("Storage must be at least %d for events but is %d",
                       (int)bytes_needed, sizeof(s_ram_backed_events_region));
  }

  memfault_trace_event_boot(evt_storage);
  memfault_reboot_tracking_collect_reset_info(evt_storage);

  MEMFAULT_TRACE_EVENT(chrome_init);

  return 0;
}

void memfault_init_hook(void) {
	memfault_platform_boot();
}
DECLARE_HOOK(HOOK_INIT, memfault_init_hook, HOOK_PRIO_DEFAULT);
