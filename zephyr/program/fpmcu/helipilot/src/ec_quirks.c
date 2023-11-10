/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "hooks.h"

extern void arm_core_mpu_disable(void);

/* TODO(b/394346384): De-duplicate with bloonchipper code */
static void prepare_for_sysjump_to_ec(void)
{
	/*
	 * When HW_STACK_PROTECTION is enabled on ARMv7-M microcontroller then
	 * the last 64 bytes of the stack is protected to detect stack
	 * overflows. Size of protected region must be greater than exception
	 * frame, so CPU won't overwrite some other data when exception occurs.
	 *
	 * EC uses different RAM layout than Zephyr, so it's possible that after
	 * sysjump some variables are stored in the protected region. EC also
	 * reconfigures MPU late (Zephyr does it in reset handler).
	 *
	 * Disable MPU protection to avoid problems after sysjump to EC.
	 */
	arm_core_mpu_disable();
}
DECLARE_HOOK(HOOK_SYSJUMP, prepare_for_sysjump_to_ec, HOOK_PRIO_LAST);
