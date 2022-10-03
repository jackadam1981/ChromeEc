/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
#include "software_panic.h"
#include "task.h"

void _exit(int rc)
{
	panic_puts("_exit called");
	software_panic(PANIC_SW_ASSERT, task_get_current());
}
