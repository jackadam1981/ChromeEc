/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
#include "software_panic.h"
#include "task.h"

#include <stdlib.h>


/**
 * Reboot the system.
 *
 * This function is called from libc functions such as abort() or exit().
 *
 * @param rc exit code
 */
#ifndef CONFIG_ARCH_POSIX
void _exit(int rc)
#else
void _exit(int rc)
#endif
{
	panic_printf("%s called with rc: %d\n", __func__, rc);
	//software_panic(PANIC_SW_EXIT, task_get_current());
    abort();
}
