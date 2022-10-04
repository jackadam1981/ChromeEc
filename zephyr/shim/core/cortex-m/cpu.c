/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/kernel.h>

#include "cpu.h"
#include "system.h"

#define STACK_IDX_REG_LR 5
#define STACK_IDX_REG_PC 6
#define STACK_IDX_REG_PSR 7

int cpu_return_from_exception(void (*func)(void))
{
	uint32_t *msp;

	__asm__ volatile("mrs %0, msp" : "=r"(msp));

	msp[STACK_IDX_REG_LR] = 0; /* We'll never return */
	msp[STACK_IDX_REG_PC] = (uint32_t)func; /* Return to this function */
	msp[STACK_IDX_REG_PSR] = (1 << 24); /* Just set thumb mode */

	/* Return from exception using main stack */
	__asm__ volatile("bx %0" : : "r"(0xFFFFFFF9));

	/* Should never reach this */
	return EC_ERROR_UNKNOWN;
}
