/* Copyright 2026 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
#include "watchdog.h"

void watchdog_reload(void)
{
	chip_watchdog_reload();
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_SYSJUMP, watchdog_reload, HOOK_PRIO_LAST);
