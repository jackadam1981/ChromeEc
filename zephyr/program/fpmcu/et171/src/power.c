/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>


static void dn_thread_entry(void *arg1, void *arg2, void *arg3)
{
	while(1) {
		printk("DN 1 thread\n");
		k_msleep(100);
	}
}

K_THREAD_DEFINE(dn_thread, 1024, dn_thread_entry, NULL, NULL, NULL, 80, 0, 0);
