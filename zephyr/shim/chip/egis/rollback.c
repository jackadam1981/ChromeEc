// Copyright 2025 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "system.h"

#include <zephyr/arch/riscv/pmp.h>

int mpu_lock_rollback(int lock)
{
	if (lock) {
		z_riscv_pmp_change_permissions(0, PMP_R);
	} else {
		z_riscv_pmp_change_permissions(0, PMP_R);
	}
	// printk("The size of sizeof(struct _thread_arch) is: %d",
	// sizeof(struct _thread_arch));
	return 0;
}
