/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include "hooks.h"

extern void arm_core_mpu_disable(void);

void trng_rand_bytes(void *buffer, size_t len)
{
	for (int i = 0; i < len; i++) {
		((uint8_t*)buffer)[i] = (uint8_t)sys_clock_cycle_get_32();
	}
}

int mkbp_set_host_active_via_custom(int active, uint32_t *timestamp){
	return 0;
}

// TODO without that, coping the ramfunc fails. The CONFIG_INIT_ARCH_HW_AT_BOOT=y should handle that but it doesn't
static void prepare_for_sysjump_to_ec(void)
{
	arm_core_mpu_disable();
}
DECLARE_HOOK(HOOK_SYSJUMP, prepare_for_sysjump_to_ec, HOOK_PRIO_LAST);
