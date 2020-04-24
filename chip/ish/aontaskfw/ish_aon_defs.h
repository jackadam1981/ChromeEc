/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_ISH_AON_DEFS_H
#define __CROS_EC_ISH_AON_DEFS_H

#include "ia_structs.h"

/* aontask entry point function */
void ish_aon_main(void);

/**
 * 8 bytes reserved on stack, just for GDB to show the correct stack
 * information when doing source code level debuging
 */
#define AON_SP_RESERVED (8)

/* TSS segment for aon task */
struct tss_entry aon_tss = {
	.prev_task_link = 0,
	.reserved1 = 0,
	.esp0 = (uint8_t *)(CONFIG_AON_PERSISTENT_BASE - AON_SP_RESERVED),
	/* entry 1 in LDT for data segment */
	.ss0 = 0xc,
	.reserved2 = 0,
	.esp1 = 0,
	.ss1 = 0,
	.reserved3 = 0,
	.esp2 = 0,
	.ss2 = 0,
	.reserved4 = 0,
	.cr3 = 0,
	/* task excute entry point */
	.eip = (uint32_t)&ish_aon_main,
	.eflags = 0,
	.eax = 0,
	.ecx = 0,
	.edx = 0,
	.ebx = 0,
	/* set stack top pointer at the end of usable aon memory */
	.esp = CONFIG_AON_PERSISTENT_BASE - AON_SP_RESERVED,
	.ebp = CONFIG_AON_PERSISTENT_BASE - AON_SP_RESERVED,
	.esi = 0,
	.edi = 0,
	/* entry 1 in LDT for data segment */
	.es = 0xc,
	.reserved5 = 0,
	/* entry 0 in LDT for code segment */
	.cs = 0x4,
	.reserved6 = 0,
	/* entry 1 in LDT for data segment */
	.ss = 0xc,
	.reserved7 = 0,
	/* entry 1 in LDT for data segment */
	.ds = 0xc,
	.reserved8 = 0,
	/* entry 1 in LDT for data segment */
	.fs = 0xc,
	.reserved9 = 0,
	/* entry 1 in LDT for data segment */
	.gs = 0xc,
	.reserved10 = 0,
	.ldt_seg_selector = 0,
	.reserved11 = 0,
	.trap_debug = 0,

	/**
	 * TSS's limit specified as 0x67, to allow the task has permission to
	 * access I/O port using IN/OUT instructions,'iomap_base_addr' field
	 * must be greater than or equal to TSS' limit
	 * see 'I/O port permissions' on
	 *	https://en.wikipedia.org/wiki/Task_state_segment
	 */
	.iomap_base_addr = GDT_DESC_TSS_LIMIT
};

#ifdef CONFIG_ISH_IPAPG
extern int ipapg(void);
extern void pg_exit_restore_ctx(void);
extern void pg_exit_save_ctx(void);
#else
static int ipapg(void)
{
	return 0;
}
static void pg_exit_restore_ctx(void)
{
}
static void pg_exit_save_ctx(void)
{
}
#endif

struct gdt_header mainfw_gdt;
uint16_t tr;

#define FPU_REG_SET_SIZE 108
uint8_t fpu_reg_set[FPU_REG_SET_SIZE];

#endif /* __CROS_EC_ISH_AON_DEFS_H */
