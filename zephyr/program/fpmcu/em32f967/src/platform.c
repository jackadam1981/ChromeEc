/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include "hooks.h"

extern void arm_core_mpu_disable(void);

static void prepare_for_sysjump_to_ec(void)
{
#ifdef CONFIG_MPU
	arm_core_mpu_disable();
#endif
}
DECLARE_HOOK(HOOK_SYSJUMP, prepare_for_sysjump_to_ec, HOOK_PRIO_LAST);

int write_protect_is_asserted_custom(void)
{
	return 0;
}
