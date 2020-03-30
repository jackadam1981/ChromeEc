/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Control and status register */

/* TODO: move to core/riscv-rv32i? */

#ifndef __CROS_EC_CSR_H
#define __CROS_EC_CSR_H

#include "common.h"

#define SET_CSR(reg, bit) ({ \
	unsigned long __tmp; \
	if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
		asm volatile ("csrrsi %0, %1, %2" : "=r"(__tmp) : "i"(reg), "i"(bit)); \
	else \
		asm volatile ("csrrs %0, %1, %2" : "=r"(__tmp) : "i"(reg), "r"(bit)); \
	__tmp; \
})

#define CLEAR_CSR_RAW(reg, bit) ({ \
	unsigned long __tmp; \
	if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
		asm volatile ("csrrci %0, " #reg ", %1" : "=r"(__tmp) : "i"(bit)); \
	else \
		asm volatile ("csrrc %0, " #reg ", %1" : "=r"(__tmp) : "r"(bit)); \
	__tmp; \
})

/* centralized control enable */
#define CSR_MCTREN		(0x7c0)
/* I$, D$, ITCM, DTCM, BTB, RAS, VIC, CG, mpu */
#define   CSR_MCTREN_ICACHE	BIT(0)
#define   CSR_MCTREN_DCACHE	BIT(1)
#define   CSR_MCTREN_ITCM	BIT(2)
#define   CSR_MCTREN_DTCM	BIT(3)
#define   CSR_MCTREN_BTB	BIT(4)
#define   CSR_MCTREN_RAS	BIT(5)
#define   CSR_MCTREN_VIC	BIT(6)
#define   CSR_MCTREN_CG		BIT(7)
#define   CSR_MCTREN_MPU	BIT(8)

/* VIC */
#define CSR_VIC_MIMASK_G0	(0x5d8)
#define CSR_VIC_MILSEL_G0	(0x5e8)
#define CSR_VIC_MIWAKEUP_G0	(0x5e0)

/* MIE (machine interrupt enable) */
#define MIP_SSIP		BIT(1)
#define MIP_HSIP		BIT(2)
#define MIP_MSIP		BIT(3)
#define MIP_STIP		BIT(5)
#define MIP_HTIP		BIT(6)
#define MIP_MTIP		BIT(7)
#define MIP_SEIP		BIT(9)
#define MIP_HEIP		BIT(10)
#define MIP_MEIP		BIT(11)
#define MIP_LTIP		BIT(16)
#define MIP_AXI			BIT(17)
#define MIP_LINT_0		BIT(24)
#define MIP_LINT_1		BIT(25)
#define MIP_LINT_2		BIT(26)
#define MIP_LINT_3		BIT(27)
#define MIP_LINT_4		BIT(28)
#define MIP_LINT_5		BIT(29)
#define MIP_LINT_6		BIT(30)
#define MIP_LINT_7		BIT(31)

#endif /* __CROS_EC_CSR_H */
