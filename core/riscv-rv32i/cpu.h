/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Registers map and definitions for RISC-V cores
 */

#ifndef __CROS_EC_CPU_H
#define __CROS_EC_CPU_H

#include <stdint.h>

/* write Exception Program Counter register */
static inline void set_mepc(uint32_t val)
{
	asm volatile ("csrw mepc, %0" : : "r"(val));
}

/* read Exception Program Counter register */
static inline uint32_t get_mepc(void)
{
	uint32_t ret;

	asm volatile ("csrr %0, mepc" : "=r"(ret));
	return ret;
}

/* read Trap cause register */
static inline uint32_t get_mcause(void)
{
	uint32_t ret;

	asm volatile ("csrr %0, mcause" : "=r"(ret));
	return ret;
}

/* Generic CPU core initialization */
void cpu_init(void);

#endif /* __CROS_EC_CPU_H */
