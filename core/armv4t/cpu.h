/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Registers map and defintions for ARMv4 cores
 */

#ifndef __CPU_H
#define __CPU_H

#include <stdint.h>

/* Program Status Register bits */
#define PSR_M_MASK      (0x1F << 0) /* CPU mode mask */
#define PSR_F           (1 << 6)    /* disable FIQ */
#define PSR_I           (1 << 7)    /* disable IRQ */
#define PSR_A           (1 << 8)    /* disable imprecise Abort */

/* write Program Status Register control bits */
static inline void set_cpsr_c(uint32_t val)
{
	asm volatile ("msr cpsr_c, %0" : : "r"(val));
}

/* read Program Status Register */
static inline uint32_t get_cpsr(void)
{
	uint32_t ret;
	asm volatile ("mrs %0, cpsr" : "=r"(ret));
	return ret;
}

/* Generic CPU core initialization */
void cpu_init(void);

#endif /* __CPU_H */
