/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <common.h>
#include <config_chip.h>
#include <power_mgt.h>
#include "ia_structs.h"
#include "ish_aon_share.h"

void ish_aon_main(void);

/**
 * 8 bytes reserved on stack, just for GDB to show the correct stack
 * information when doing source code level debuging
 */
#define AON_SP_RESERVED (8)

/* TSS segment for aon task */
static struct tss_entry aon_tss = {
	.prev_task_link = 0,
	.reserved1 = 0,
	.esp0 = (uint8_t *)(CONFIG_ISH_AON_SRAM_ROM_START - AON_SP_RESERVED),
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
	.esp = CONFIG_ISH_AON_SRAM_ROM_START,
	.ebp = AON_SP_RESERVED,
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

	/* TSS's limit specified as 0x67, to allow the task has permission to
	 * access I/O port using IN/OUT instructions,'iomap_base_addr' field
	 * must be greater than or equal to TSS' limit
	 * see 'I/O port permissions' on
	 *	https://en.wikipedia.org/wiki/Task_state_segment
	 */
	.iomap_base_addr = GDT_DESC_TSS_LIMIT
};

/**
 * define code and data LDT segements for aontask
 * code : base = 0x0, limit = 0xFFFFFFFF, Present = 1, DPL = 0
 * data : base = 0x0, limit = 0xFFFFFFFF, Present = 1, DPL = 0
 */
static struct ldt_entry aon_ldt[2] = {

	/* entry 0 for code segment
	 * base: 0x0
	 * limit: 0xFFFFFFFF
	 * flag: 0x9B, Present = 1, DPL = 0, code segment
	 */
	{
		.dword_lo = GEN_GDT_DESC_LO(0x0, 0xFFFFFFFF,
				GDT_DESC_CODE_FLAG),

		.dword_hi = GEN_GDT_DESC_HI(0x0, 0xFFFFFFFF,
				GDT_DESC_CODE_FLAG)
	},

	/* entry 1 for data segment
	 * base: 0x0
	 * limit: 0xFFFFFFFF
	 * flag: 0x93, Present = 1, DPL = 0, data segment
	 */
	{
		.dword_lo = GEN_GDT_DESC_LO(0x0, 0xFFFFFFFF,
				GDT_DESC_DATA_FLAG),

		.dword_hi = GEN_GDT_DESC_HI(0x0, 0xFFFFFFFF,
				GDT_DESC_DATA_FLAG)
	}
};

/* shared data structure between main FW and aon task */
struct ish_aon_share aon_share = {
	.aon_tss = &aon_tss,
	.ldt_ptr = (uint32_t)&aon_ldt,
	.ldt_size = sizeof(aon_ldt),
};

static void handle_d0i2(void)
{
	/* TODO set SRAM to retention mode*/

	/* wakeup from PMU interrupt */
	/* ish_halt(); */

	/* TODO set SRAM to normal operation mode */
}

static void handle_d0i3(void)
{
	/* TODO store main FW 's context to IMR DDR from main sram */
	/* TODO power off main SRAM */

	/* wakeup from PMU interrupt */
	/* ish_halt(); */

	/* TODO power on main SRAM */
	/* TODO restore main FW 's context to SRAM from IMR DDR */
}

static void handle_d3(void)
{
	/* TODO store main FW 's context to IMR DDR from main sram */
	/* TODO power off main SRAM */

	/* TODO handle D3 */
}

static void handle_reset_prep(void)
{
	/* TODO store main FW 's context to IMR DDR from main sram */
	/* TODO power off main SRAM */

	/* TODO handle reset prep */
}

static void handle_unknown_state(void)
{
	/* TODO store main FW 's context to IMR DDR from main sram */
	/* TODO power off main SRAM */

	/* TODO handle unknown state */
}

void ish_aon_main(void)
{
	/* TODO reset IDT */

	while (1) {
		switch (aon_share.pm_state) {
		case ISH_PM_STATE_D0I2:
			handle_d0i2();
			break;
		case ISH_PM_STATE_D0I3:
			handle_d0i3();
			break;
		case ISH_PM_STATE_D3:
			handle_d3();
			break;
		case ISH_PM_STATE_RESET_PREP:
			handle_reset_prep();
			break;
		default:
			handle_unknown_state();
			break;
		}

		/* switch back to main FW */
		__asm__ volatile("iret ;\n\t");
	}
}
