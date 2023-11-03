/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "debug.h"
#include "stdbool.h"

#include "task.h"
#include "software_panic.h"
#include "panic.h"

__overridable bool debugger_is_connected(void)
{
	return false;
}

__overridable bool debugger_was_connected(void)
{
	return false;
}

__overridable void debugger_enable_disable(bool enable)
{
	/* This should never be called on a platform that doesn't implement it. */
	software_panic(PANIC_SW_ASSERT, task_get_current());
}
