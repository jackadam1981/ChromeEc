/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the N8 core
 */

#include "cpu.h"
#include "registers.h"

void cpu_init(void)
{
#ifdef CONFIG_FPU
#error "Please use CONFIG_FPU_IT83XX instead of CONFIG_FPU"
#endif
	/* DLM initialization is done in init.S */
	/* Global interrupt enable */
	asm volatile ("setgie.e");
}
