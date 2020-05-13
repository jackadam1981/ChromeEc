/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Control and status register */

/* TODO: move to core/riscv-rv32i? */

#ifndef __CROS_EC_CSR_H
#define __CROS_EC_CSR_H

#include "common.h"

#define READ_CSR(reg) ({ \
	unsigned long __tmp; \
	asm volatile("csrr %0, %1" : "=r"(__tmp) : "i"(reg)); \
	__tmp; \
})

#define WRITE_CSR(reg, val) ({ \
	if (__builtin_constant_p(val) && (unsigned long)(val) < 32) \
		asm volatile ("csrwi %0, %1" :: "i"(reg), "i"(val)); \
	else \
		asm volatile ("csrw %0, %1" :: "i"(reg), "r"(val)); \
})

#define SET_CSR(reg, bit) ({ \
	unsigned long __tmp; \
	if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
		asm volatile ("csrrsi %0, %1, %2" : "=r"(__tmp) : "i"(reg), "i"(bit)); \
	else \
		asm volatile ("csrrs %0, %1, %2" : "=r"(__tmp) : "i"(reg), "r"(bit)); \
	__tmp; \
})

#define CLEAR_CSR(reg, bit) ({ \
	unsigned long __tmp; \
	if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
		asm volatile ("csrrci %0, %1, %2" : "=r"(__tmp) : "i"(reg), "i"(bit)); \
	else \
		asm volatile ("csrrc %0, %1, %2" : "=r"(__tmp) : "i"(reg), "r"(bit)); \
	__tmp; \
})

#define READ_CSR_RAW(reg) ({ \
	unsigned long __tmp; \
	asm volatile ("csrr %0, " #reg : "=r"(__tmp)); \
	__tmp; \
})

#define WRITE_CSR_RAW(reg, val) ({ \
	if (__builtin_constant_p(val) && (unsigned long)(val) < 32) \
		asm volatile ("csrwi " #reg ", %0" :: "i"(val)); \
	else \
		asm volatile ("csrw " #reg ", %0" :: "r"(val)); \
})

#define SET_CSR_RAW(reg, bit) ({ \
	unsigned long __tmp; \
	if (__builtin_constant_p(bit) && (unsigned long)(bit) < 32) \
		asm volatile ("csrrsi %0, " #reg ", %1" : "=r"(__tmp) : "i"(bit)); \
	else \
		asm volatile ("csrrs %0, " #reg ", %1" : "=r"(__tmp) : "r"(bit)); \
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
#define CSR_VIC_MICAUSE		(0x5c0)
#define CSR_VIC_MIEMS		(0x5c2)
#define CSR_VIC_MIPEND_G0	(0x5d0)
#define CSR_VIC_MIPEND_G1	(0x5d1)
#define CSR_VIC_MIMASK_G0	(0x5d8)
#define CSR_VIC_MIWAKEUP_G0	(0x5e0)
#define CSR_VIC_MILSEL_G0	(0x5e8)

#endif /* __CROS_EC_CSR_H */
