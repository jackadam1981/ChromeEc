/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

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
	printk("%s called with rc: %d\n", __func__, rc);
	k_thread_abort(k_current_get());
}
