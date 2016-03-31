/* Copyright (c) 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Registers map and defintions for mIA LM2 processor
 */

#ifndef __CROS_EC_IA32_INTERRUPTS_H
#define __CROS_EC_IA32_INTERRUPTS_H

#include <stdint.h>

#define USHRT_MAX		0xFFFF
typedef struct {
	unsigned irq;
	unsigned trigger;
	unsigned polarity;
	unsigned vector;
} irq_desc_t;

#define INTR_DESC(__irq,__vector,__trig)                    \
    {                                                       \
        .irq            = __irq,                            \
        .trigger        = __trig,                           \
        .polarity       = IOAPIC_REDTBL_INTPOL_HIGH,        \
        .vector         = __vector	                    \
    }

#define LEVEL_INTR(__irq, __vector) \
	INTR_DESC(__irq, __vector, IOAPIC_REDTBL_TRIGGER_LEVEL)
#define EDGE_INTR(__irq, __vector) \
	INTR_DESC(__irq, __vector, IOAPIC_REDTBL_TRIGGER_EDGE)


/* Interrupt descriptor entry */
struct IDT_entry_t {
	uint16_t ISR_low;	/* Low 16 bits of handler address. */
	uint16_t segment_selector;	/* Flat model means this is not changed. */
	uint8_t zero;		/* Must be set to zero. */
	uint8_t flags;		/* Flags for this entry. */
	uint16_t ISR_high;	/* High 16 bits of handler address. */
} __attribute__ ((packed));
typedef struct IDT_entry_t IDT_entry;

/* IDT pointer */
struct IDT_pointer_t {
	uint16_t table_limit;
	uint32_t table_base;
} __attribute__ ((__packed__));
typedef struct IDT_pointer_t IDT_pointer;

#define NUM_VECTORS			256
#define DEST_APIC_ID			0
/* Default flags setting for entries in the IDT. */
#define IDT_FLAGS			(0x8E)

/* The interrupt priority (for vectors 16 to 255) is determined by vector/16.
 * The quotient is rounded to the nearest integer with 1 being the lowest priority
 * and 15 is the highest.  Therefore the following two interrupts are at the lowest
 * priority.  *NOTE 1* If the yield vector is changed then it must also be changed
 * in the portYIELD_INTERRUPT definition immediately below. */
#define APIC_YIELD_INT_VECTOR		(0x20)
#define APIC_LVT_ERROR_VECTOR		(0x21)
#define APIC_SPURIOUS_INT_VECTOR	(0xff)
/* This is the lowest possible ISR vector available to application code. */
#define APIC_MIN_ALLOWABLE_VECTOR	(0x20)
#define APIC_DIV_16			(0x03)

typedef uint32_t TickType_t;
typedef void (*isr_handler_t) (void);

#define CPU_CLOCK_HZ		((unsigned long) 66000000) /* = 66.000MHz clk gen */
#define TICK_RATE_HZ		((TickType_t) 1000)

/* APIC bit definitions. */
#define APIC_ENABLE_BIT			(1UL << 8UL)

#define ISR_STACK_SIZE           	512
#define BYTE_ALIGNMENT_MASK		(0x0003)
#define APIC_BASE			0xFEE00000UL
#define APIC_SPURIOUS_INT		REG32(APIC_BASE + 0xF0UL )
#define APIC_LVT_ERROR			REG32(APIC_BASE + 0x370UL)

void default_int_handler(void);
void init_interrupts(void);
void mask_interrupt(unsigned int irq);
void unmask_interrupt(unsigned int irq);

#endif	/* __CROS_EC_IA32_INTERRUPTS_H */
