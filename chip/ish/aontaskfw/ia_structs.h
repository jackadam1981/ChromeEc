/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_IA_STRUCTS_H
#define __CROS_EC_IA_STRUCTS_H

#include <stdint.h>

/**
 * IA32/x86 architecture related data structure definitions.
 * including: Global Descriptor Table (GDT), Local Descriptor Table (LDT),
 * Interrupt Descriptor Table (IDT) and Task State Segment (TSS)
 * see: https://en.wikipedia.org/wiki/Global_Descriptor_Table
 *      https://en.wikipedia.org/wiki/Interrupt_descriptor_table
 *      https://en.wikipedia.org/wiki/Task_state_segment
 */


/* GDT entry descriptor */
struct gdt_entry {
	uint16_t limit_lw;	/* bits 0:15 of segment limit */
	uint16_t base_addr_lw;	/* bits 0:15 of segment base address */
	uint8_t base_addr_mb;	/* bits 16:23 of segment base address */
	uint8_t type;		/* descriptor type fields */
	uint8_t limit_ub;	/* bits 16:19 of limit + more type fields */
	uint8_t base_addr_ub;	/* bits 24:31 of segment base address */
};

/* GDT header */
struct gdt_header {
	uint16_t limit;			/* GDT limit */
	struct gdt_entry *p_entries;	/* pointer to GDT entries */
} __packed;

/* LDT entry descriptor */
struct ldt_entry {
	uint32_t dword_lo;	/* lower end dword of a entry descriptor */
	uint32_t dword_hi;	/* high end dword of a entry descriptor */
};

/* IDT entry descriptor */
struct idt_entry {
	uint32_t dword_lo;	/* lower end dword of a entry descriptor */
	uint32_t dword_hi;	/* high end dword of a entry descriptor  */
};

/* IDT header */
struct idt_header {
	uint16_t size;		/* IDT size */
	uint32_t start;		/* start address of IDT */
} __packed;


/* TSS entry descriptor */
struct tss_entry {
	uint16_t prev_task_link;
	uint16_t reserved1;
	char *esp0;
	uint16_t ss0;
	uint16_t reserved2;
	char *esp1;
	uint16_t ss1;
	int16_t reserved3;
	char *esp2;
	uint16_t ss2;
	uint16_t reserved4;
	uint32_t cr3;
	int32_t eip;
	uint32_t eflags;
	uint32_t eax;
	uint32_t ecx;
	int32_t edx;
	uint32_t ebx;
	uint32_t esp;
	uint32_t ebp;
	uint32_t esi;
	uint32_t edi;
	int16_t es;
	uint16_t reserved5;
	uint16_t cs;
	uint16_t reserved6;
	uint16_t ss;
	uint16_t reserved7;
	uint16_t ds;
	uint16_t reserved8;
	uint16_t fs;
	uint16_t reserved9;
	uint16_t gs;
	uint16_t reserved10;
	uint16_t ldt_seg_selector;
	uint16_t reserved11;
	uint16_t trap_debug;
	/* offset from TSS base for I/O perms */
	uint16_t iomap_base_addr;
} __packed;

/* code segment flag,  E/R, Present = 1, DPL = 0, Acesssed = 1 */
#define GDT_DESC_CODE_FLAG	(0x9B)

/* data segment flag,  R/W, Present = 1, DPL = 0, Acesssed = 1 */
#define GDT_DESC_DATA_FLAG	(0x93)

/* TSS segment limit size */
#define GDT_DESC_TSS_LIMIT	(0x67)

/* TSS segment flag, Present = 1, DPL = 0, Acesssed = 1 */
#define GDT_DESC_TSS_FLAG	(0x89)

/* LDT segment flag, Present = 1, DPL = 0 */
#define GDT_DESC_LDT_FLAG	(0x82)

/* macros helper to create a GDT entry descriptor
 * set default limit unit is 4096-byte pages for granularity
 */
#define GEN_GDT_DESC_LO(base, limit, flags)                                    \
	((((limit) >> 12) & 0xFFFF) | (((base)&0xFFFF) << 16))

#define GEN_GDT_DESC_HI(base, limit, flags)                                    \
	((((base) >> 16) & 0xFF) | (((flags) << 8) & 0xFF00) |                 \
	(((limit) >> 12) & 0xFF0000) | ((base)&0xFF000000) | 0xc00000)


#endif /* __CROS_EC_IA_STRUCTS_H */
