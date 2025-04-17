/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "hooks.h"
static void active_task(void)
{
	k_busy_wait(500000);
}
DECLARE_HOOK(HOOK_TICK, active_task, HOOK_PRIO_DEFAULT);
