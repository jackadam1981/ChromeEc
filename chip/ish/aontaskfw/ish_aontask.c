/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <common.h>
#include <power_mgt.h>
#include "ia_structs.h"
#include "ish_aon_share.h"

#define AON_IDT_ENTRY_VEC_FIRST        ISH_PMU_WAKEUP_VEC

#ifdef CONFIG_ISH_PM_RESET_PREP
/* assume reset prep interrupt vector is behind PMU wakeup interrupt vector */
#define AON_IDT_ENTRY_VEC_LAST         ISH_RESET_PREP_VEC
#else
#define AON_IDT_ENTRY_VEC_LAST         ISH_PMU_WAKEUP_VEC
#endif

#define _S(name)	#name
#define S(name)		_S(name)

#define CYCLES_PER_US                  100
#define SRAM_RETENTION_US_DELAY	       5
#define SRAM_RETENTION_CYCLES_DELAY    (SRAM_RETENTION_US_DELAY * CYCLES_PER_US)


/* ISR for PMU wakeup interrupt */
static void pmu_wakeup_isr(void)
{
	__asm__ volatile (
			"movl $"S(ISH_PMU_WAKEUP_VEC)", "S(IOAPIC_EOI_REG)";\n"
			"movl $0x00, "S(LAPIC_EOI_REG)";\n"
			"iret;"
			);

	__builtin_unreachable();
}

#ifdef CONFIG_ISH_PM_RESET_PREP

/* ISR for reset prep  interrupt */
static void reset_prep_isr(void)
{
	__asm__ volatile (
			"movl $"S(ISH_RESET_PREP_VEC)", "S(IOAPIC_EOI_REG)";\n"
			"movl $0x00, "S(LAPIC_EOI_REG)";\n"
			"iret;"
			);

	__builtin_unreachable();
}

static struct idt_entry aon_idt[2];

#else

static struct idt_entry aon_idt[1];

#endif

/**
 * setting IDT header for IDTR register, since we only allocated spaces for
 * PMU wakeup interrupt gate IDT entry and reset prep interrupt gate IDT entry
 * (if CONFIG_ISH_PM_RESET_PREP), the 'start' filed still need to point to the
 * entry 0.
 */
static struct idt_header aon_idt_hdr = {

	.size = (sizeof(struct idt_entry) * (AON_IDT_ENTRY_VEC_LAST + 1)) - 1,
	.start = ((uint32_t)&aon_idt -
			(sizeof(struct idt_entry) * AON_IDT_ENTRY_VEC_FIRST))
};

/* aontask entry point function */
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
	.esp = CONFIG_ISH_AON_SRAM_ROM_START - AON_SP_RESERVED,
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

	/**
	 * TSS's limit specified as 0x67, to allow the task has permission to
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

	/**
	 * entry 0 for code segment
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

	/**
	 * entry 1 for data segment
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
	/* set SRAM to retention mode*/
	PMU_LDO_CTRL = 	PMU_LDO_BIT_RETENTION_ON | PMU_LDO_BIT_ON;

	__sync_synchronize();

	/* delay some cycles */
	__asm__ volatile (
			"movl $"S(SRAM_RETENTION_CYCLES_DELAY)", %%ecx;\n"
			"loop .;"
			:
			:
			: "ecx"
			);

	ish_halt();

	/* wakeup from PMU interrupt */

	/* set SRAM to normal mode */
	PMU_LDO_CTRL = 	PMU_LDO_BIT_ON;

	/**
	 * poll LDO_READY status to make sure SRAM LDO is on
	 * (exited retention mode)
	 */
	while (!(PMU_LDO_CTRL & PMU_LDO_BIT_READY));
}

static void handle_d0i3(void)
{
	/* TODO store main FW 's context to IMR DDR from main sram */
	/* TODO power off main SRAM */

	ish_halt();
	/* wakeup from PMU interrupt */

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

	/* set PMU wakeup interrupt gate using LDT code segment selector(0x4) */
	aon_idt[0].dword_lo = GEN_IDT_DESC_LO(&pmu_wakeup_isr, 0x4,
					IDT_DESC_FLAG);

	aon_idt[0].dword_hi = GEN_IDT_DESC_HI(&pmu_wakeup_isr, 0x4,
					IDT_DESC_FLAG);
#ifdef CONFIG_ISH_PM_RESET_PREP

	/* set reset prep interrupt gate using LDT code segment selector(0x4) */
	aon_idt[1].dword_lo = GEN_IDT_DESC_LO(&reset_prep_isr, 0x4,
					IDT_DESC_FLAG);

	aon_idt[1].dword_hi = GEN_IDT_DESC_HI(&reset_prep_isr, 0x4,
					IDT_DESC_FLAG);
#endif

	/* save main FW's IDT and load aontask's IDT */
	__asm__ volatile (
			"sidtl %0;\n"
			"lidtl %1;\n"
			:
			: "m" (aon_share.main_fw_idt_hdr), "m" (aon_idt_hdr)
			);

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

		/* restore main FW's IDT and switch back to main FW */
		__asm__ volatile(
				"lidtl %0;\n"
				"iret;"
				:
				: "m" (aon_share.main_fw_idt_hdr)
				);
	}
}
