/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Set up the LM2 mIA core & interrupts
 */

#include "common.h"
#include "util.h"
#include "interrupts.h"
#include "registers.h"
#include "task_defs.h"
#include "irq_handler.h"
#include "console.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_SYSTEM, outstr)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)

/* The IDT itself. */
__attribute__ ((aligned(32)))
IDT_entry __idt[NUM_VECTORS];
IDT_pointer __idt_ptr;

/* To count the interrupt nesting depth. Usually it is not nested */
volatile uint32_t __in_isr __attribute__ ((used)) = 0;

/* The stack used by interrupt handlers. */
static uint32_t __isr_stack[ISR_STACK_SIZE] __attribute__ ((used)) = { 0};
volatile uint32_t __isr_stack_ptr __attribute__ ((used)) =
    (uint32_t) & (__isr_stack[ISR_STACK_SIZE - 5]);


void write_ioapic_reg(const uint32_t reg, const uint32_t val)
{
	REG32(IOAPIC_IDX) = (uint8_t)reg;
	REG32(IOAPIC_WDW) = val;
}

uint32_t read_ioapic_reg(const uint32_t reg)
{
	REG32(IOAPIC_IDX) = (uint8_t)reg;
	return REG32(IOAPIC_WDW);
}

void set_ioapic_redtbl_raw(const unsigned irq, const uint32_t val)
{
	/* [IOAPIC], 3.2.4. "IOREDTBL[23:0] I/O REDIRECTION TABLE REGISTERS" */
	const uint32_t redtbl_lo = IOAPIC_IOREDTBL + 2 * irq;
	const uint32_t redtbl_hi = redtbl_lo + 1;

	write_ioapic_reg(redtbl_lo, val);
	write_ioapic_reg(redtbl_hi, DEST_APIC_ID);
}

void set_ioapic_redtbl(irq_desc_t irq_desc)
{
	uint32_t val = irq_desc.vector |
	    IOAPIC_REDTBL_DELMOD_FIXED |
	    IOAPIC_REDTBL_DESTMOD_PHYS | irq_desc.polarity | irq_desc.trigger;

	set_ioapic_redtbl_raw(irq_desc.irq, val);
}

void unmask_interrupt(uint32_t irq)
{
	const uint32_t redtbl_lo = IOAPIC_IOREDTBL + 2 * irq;
	uint32_t val = read_ioapic_reg(redtbl_lo);
	val &= ~IOAPIC_REDTBL_MASK;
	set_ioapic_redtbl_raw(irq, val);
}

void mask_interrupt(uint32_t irq)
{
	const uint32_t redtbl_lo = IOAPIC_IOREDTBL + 2 * irq;
	uint32_t val = read_ioapic_reg(redtbl_lo);
	val |= IOAPIC_REDTBL_MASK;
	set_ioapic_redtbl_raw(irq, val);
}

/* Maps IRQs to vectors. To be programmed in IOAPIC redirection table */
irq_desc_t system_irqs[] = {
	LEVEL_INTR(ISH30_I2C0_IRQ, I2C0_VEC),
	LEVEL_INTR(ISH30_I2C1_IRQ, I2C1_VEC),
	LEVEL_INTR(ISH30_I2C2_IRQ, I2C2_VEC),
	LEVEL_INTR(ISH30_IPC_IRQ_HOST2ISH, IPC_VEC),
	LEVEL_INTR(ISH30_HPET_TIMER0_IRQ, HPET_TIMER0_VEC),
	LEVEL_INTR(ISH30_HPET_TIMER1_IRQ, HPET_TIMER12_VEC),
};

void init_ioapic(void)
{
	unsigned entry;
	unsigned num_system_irqs = sizeof(system_irqs)/sizeof(irq_desc_t);
	unsigned max_entries = (read_ioapic_reg(IOAPIC_VERSION) >> 16) & 0xff;

	for (entry = 0; entry < max_entries; entry++) {
		set_ioapic_redtbl_raw(entry, IOAPIC_REDTBL_MASK);
	}

	for (entry = 0; entry < num_system_irqs; entry++) {
		set_ioapic_redtbl_raw(system_irqs[entry].irq,
				      system_irqs[entry].vector |
				      IOAPIC_REDTBL_DELMOD_FIXED |
				      IOAPIC_REDTBL_DESTMOD_PHYS |
				      IOAPIC_REDTBL_MASK |
				      system_irqs[entry].polarity |
				      system_irqs[entry].trigger);
	}
}

void set_interrupt_gate(uint8_t num, isr_handler_t func, uint8_t flags)
{
	uint16_t code_segment;
	uint32_t base = (uint32_t) func;

	__idt[num].ISR_low =
	    (uint16_t) (base & USHRT_MAX);
	__idt[num].ISR_high =
	    (uint16_t) ((base >> 16UL) & USHRT_MAX);

	/* When the flat model is used the CS will never change. */
	__asm volatile ("mov %%cs, %0":"=r" (code_segment));
	__idt[num].segment_selector = code_segment;
	__idt[num].zero = 0;
	__idt[num].flags = flags;
}

/* Need to move this into individual drivers */
DECLARE_IRQ(ISH30_I2C0_IRQ, i2c_isr_bus0, I2C0_VEC);
DECLARE_IRQ(ISH30_I2C1_IRQ, i2c_isr_bus1, I2C1_VEC);
DECLARE_IRQ(ISH30_I2C2_IRQ, i2c_isr_bus2, I2C2_VEC);
DECLARE_IRQ(ISH30_IPC_IRQ_HOST2ISH, ipc_interrupt_handler, IPC_VEC);
DECLARE_IRQ(ISH30_HPET_TIMER0_IRQ, __hw_clock_source_irq_0, HPET_TIMER0_VEC);
DECLARE_IRQ(ISH30_HPET_TIMER1_IRQ, __hw_clock_source_irq_1, HPET_TIMER12_VEC);

/* IDT vectors can be loaded across files but setting up
 * interrupt gates may not */
void register_vec_to_handler(void)
{
	set_interrupt_gate(I2C0_VEC, IRQ_HANDLER(ISH30_I2C0_IRQ), IDT_FLAGS);
	set_interrupt_gate(I2C1_VEC, IRQ_HANDLER(ISH30_I2C1_IRQ), IDT_FLAGS);
	set_interrupt_gate(I2C2_VEC, IRQ_HANDLER(ISH30_I2C2_IRQ), IDT_FLAGS);
	set_interrupt_gate(IPC_VEC, IRQ_HANDLER(ISH30_IPC_IRQ_HOST2ISH),
				IDT_FLAGS);
	set_interrupt_gate(HPET_TIMER0_VEC, IRQ_HANDLER(ISH30_HPET_TIMER0_IRQ),
				IDT_FLAGS);
	set_interrupt_gate(HPET_TIMER12_VEC, IRQ_HANDLER(ISH30_HPET_TIMER1_IRQ),
				IDT_FLAGS);
}

void unhandled_vector(void)
{
	uint32_t vec = 0xf, i;
	uint32_t ioapic_icr_last = LAPIC_ISR; /* In service register */

	/* Scan ISRs */
	for (i = 7; i >= 0; i--, ioapic_icr_last -= 0x10) {

		asm("movl (%1), %0\n" : "=&r" (vec) : "r" (ioapic_icr_last));
		if (vec) {
			vec = (32 * __fls(vec)) + i;
			break;
		}
	}

	CPRINTF("Ignoring vector 0x%0x!\n", vec);
	asm("" :: "a" (vec));
}

void init_interrupts(void)
{

	__isr_stack_ptr &= ~BYTE_ALIGNMENT_MASK;

	register_vec_to_handler();

	init_ioapic();

	set_interrupt_gate(APIC_LVT_ERROR_VECTOR, default_int_handler,
				IDT_FLAGS);
	set_interrupt_gate(APIC_YIELD_INT_VECTOR, __switchto, IDT_FLAGS);
	set_interrupt_gate(APIC_SPURIOUS_INT_VECTOR, default_int_handler,
				IDT_FLAGS);

	/* Enable the APIC, mapping the spurious interrupt at the same time. */
	APIC_SPURIOUS_INT = APIC_SPURIOUS_INT_VECTOR | APIC_ENABLE_BIT;

	/* Set timer error vector. */
	APIC_LVT_ERROR = APIC_LVT_ERROR_VECTOR;

	return;
}
