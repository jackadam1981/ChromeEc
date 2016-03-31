/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "hooks.h"
#include "registers.h"
#include "task.h"
#include "watchdog.h"

void watchdog_reload(void)
{
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

int watchdog_init(void)
{
	return EC_SUCCESS;
}

#ifdef CONFIG_WATCHDOG_HELP
void __keep watchdog_check(uint32_t excep_lr, uint32_t excep_sp)
{
}
#endif
