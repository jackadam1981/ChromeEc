/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "panic.h"
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
void exit(int rc)
#endif
{
	panic_printf("%s called with rc: %d\n", __func__, rc);
	abort();
}
