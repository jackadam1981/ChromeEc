/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Registers map and defintions for Andes cores
 */

#ifndef __CPU_H
#define __CPU_H

#include <stdint.h>

/* Process Status Word bits */
#define PSW_GIE		(1 << 0) /* Global Interrupt Enable */
#define PSW_INTL_SHIFT	1        /* Interrupt Stack Level */
#define PSW_INTL_MASK	(0x3 << PSW_INTL_SHIFT)

/* write Process Status Word priviledged register */
static inline void set_psw(uint32_t val)
{
	asm volatile ("mtsr %0, $PSW \n"::"r"(val));
}

/* read Process Status Word priviledged register */
static inline uint32_t get_psw(void)
{
	uint32_t ret;
	asm volatile ("mfsr %0, $PSW \n":"=r"(ret));
	return ret;
}

/* Set up the cpu to detect faults */
void cpu_init(void);

#endif /* __CPU_H */
