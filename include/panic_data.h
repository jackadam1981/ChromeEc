/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Panic handling, including displaying a message on the panic reporting
 * device, which is currently the UART.
 */

#ifndef __CROS_EC_PANIC_DATA_H
#define __CROS_EC_PANIC_DATA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ARM Cortex-Mx registers saved on panic */
struct cortex_panic_data {
	uint32_t regs[12];        /* psp, ipsr, msp, r4-r11, lr(=exc_return).
				   * In version 1, that was uint32_t regs[11] =
				   * psp, ipsr, lr, r4-r11
				   */
	uint32_t frame[8];        /* r0-r3, r12, lr, pc, xPSR */

	uint32_t mmfs;
	uint32_t bfar;
	uint32_t mfar;
	uint32_t shcsr;
	uint32_t hfsr;
	uint32_t dfsr;
};

/* NDS32 N8 registers saved on panic */
struct nds32_n8_panic_data {
	uint32_t itype;
	uint32_t regs[16];        /* r0-r10, r15, fp, gp, lp, sp */
	uint32_t ipc;
	uint32_t ipsw;
};

/* RISC-V RV32I registers saved on panic */
struct rv32i_panic_data {
	uint32_t regs[31];        /* sp, ra, gp, tp, a0-a7, t0-t6 s0-s11 */
	uint32_t mepc;            /* mepc */
	uint32_t mcause;          /* mcause */
};

/* x86 registers saved on panic */
struct x86_panic_data {
	uint32_t vector;          /* Exception vector number */

	/* Data pushed when exception handler called */
	uint32_t error_code;
	uint32_t eip;
	uint32_t cs;
	uint32_t eflags;

	/* General purpose registers */
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
	uint32_t esi;
	uint32_t edi;

	/* Task id at time of panic */
	uint8_t task_id;
};

/* Data saved across reboots */
struct panic_data {
	uint8_t arch;             /* Architecture (PANIC_ARCH_*) */
	uint8_t struct_version;   /* Structure version (currently 2) */
	uint8_t flags;            /* Flags (PANIC_DATA_FLAG_*) */
	uint8_t reserved;         /* Reserved; set 0 */

	/* core specific panic data */
	union {
		struct cortex_panic_data cm;       /* Cortex-Mx registers */
		struct nds32_n8_panic_data nds_n8; /* NDS32 N8 registers */
		struct x86_panic_data x86;         /* Intel x86 */
		struct rv32i_panic_data riscv;     /* RISC-V RV32I */
	};

	/*
	 * These fields go at the END of the struct so we can find it at the
	 * end of memory.
	 */
	uint32_t struct_size;     /* Size of this struct */
	uint32_t magic;           /* PANIC_SAVE_MAGIC if valid */
};

#define PANIC_DATA_MAGIC 0x21636e50  /* "Pnc!" */
enum panic_arch {
	PANIC_ARCH_CORTEX_M = 1,     /* Cortex-M architecture */
	PANIC_ARCH_NDS32_N8 = 2,     /* NDS32 N8 architecture */
	PANIC_ARCH_X86 = 3,          /* Intel x86 */
	PANIC_ARCH_RISCV_RV32I = 4,  /* RISC-V RV32I */
};

/* Use PANIC_DATA_PTR to refer to the persistent storage location */
#define PANIC_DATA_PTR ((struct panic_data *)CONFIG_PANIC_DATA_BASE)

/* Flags for panic_data.flags */
/* panic_data.frame is valid */
#define PANIC_DATA_FLAG_FRAME_VALID    BIT(0)
/* Already printed at console */
#define PANIC_DATA_FLAG_OLD_CONSOLE    BIT(1)
/* Already returned via host command */
#define PANIC_DATA_FLAG_OLD_HOSTCMD    BIT(2)
/* Already reported via host event */
#define PANIC_DATA_FLAG_OLD_HOSTEVENT  BIT(3)

#ifdef __cplusplus
}
#endif

#endif  /* __CROS_EC_PANIC_DATA_H */
