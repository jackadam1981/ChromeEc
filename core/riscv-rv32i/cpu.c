/* Copyright 2019 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the RISC-V core
 */

#include "common.h"
#include "cpu.h"

void cpu_init(void)
{
	/* bit3: Global interrupt enable (M-mode) */
	asm volatile("csrsi mstatus, 0x8");
}

__noreturn void cpu_undefined_instruction(void)
{
	asm volatile("unimp");
	__builtin_unreachable();
}
