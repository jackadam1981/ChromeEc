/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Port 80 Timer Interrupt for MEC17XX */

#include "common.h"
#include "console.h"
#include "hooks.h"
#include "lpc.h"
#include "port80.h"
#include "registers.h"
#include "task.h"


#if 0

/* Fire timer interrupt every 1000 usec to check for port80 data. */
#define POLL_PERIOD_USEC 1000
/* After 30 seconds of no port 80 data, disable the timer interrupt. */
#define INTERRUPT_DISABLE_TIMEOUT_SEC 30
#define INTERRUPT_DISABLE_IDLE_COUNT (INTERRUPT_DISABLE_TIMEOUT_SEC \
				      * 1000000 \
				      / POLL_PERIOD_USEC)

/* Count the number of consecutive interrupts with no port 80 data. */
static int idle_count;

static void port_80_interrupt_enable(void)
{
	idle_count = 0;

	/* Enable the interrupt. */
	task_enable_irq(MEC17XX_IRQ_TIMER16_1);
	/* Enable and start the timer. */
	MEC17XX_TMR16_CTL(1) |= 1 | (1 << 5);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, port_80_interrupt_enable, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_RESET, port_80_interrupt_enable, HOOK_PRIO_DEFAULT);

static void port_80_interrupt_disable(void)
{
	/* Disable the timer block. */
	MEC17XX_TMR16_CTL(1) &= ~1;
	/* Disable the interrupt. */
	task_disable_irq(MEC17XX_IRQ_TIMER16_1);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, port_80_interrupt_disable,
	     HOOK_PRIO_DEFAULT);

/*
 * The port 80 interrupt will use TIMER16 instance 1 for a 1ms countdown
 * timer.  This timer is on GIRQ23, bit 1.
 */
static void port_80_interrupt_init(void)
{
	uint32_t val = 0;

	/*
	 * The timers are driven by a 48MHz oscillator.  Prescale down to
	 * 1MHz. 48MHz/48 -> 1MHz
	 */
	val = MEC17XX_TMR16_CTL(1);
	val = (val & 0xFFFF) | (47 << 16);
	/* Automatically restart the timer. */
	val |= (1 << 3);
	/* The counter should decrement. */
	val &= ~(1 << 2);
	MEC17XX_TMR16_CTL(1) = val;

	/* Set the reload value(us). */
	MEC17XX_TMR16_PRE(1) = POLL_PERIOD_USEC;

	/* Clear the status if any. */
	MEC17XX_TMR16_STS(1) |= 1;

	/* Clear any pending interrupt. */
	MEC17XX_INT_SOURCE(MEC17XX_TMR16_GIRQ) = MEC17XX_TMR16_GIRQ_BIT(1);
	/* Enable IRQ vector 23. */
	/* TODO not needed for direct mode interrupts
	 * MEC17XX_INT_BLK_EN = (1 << MEC17XX_TMR16_GIRQ);
	 */
	/* Enable the interrupt. */
	MEC17XX_TMR16_IEN(1) |= 1;
	MEC17XX_INT_ENABLE(MEC17XX_TMR16_GIRQ) = MEC17XX_TMR16_GIRQ_BIT(1);

	port_80_interrupt_enable();
}
DECLARE_HOOK(HOOK_INIT, port_80_interrupt_init, HOOK_PRIO_DEFAULT);

void port_80_interrupt(void)
{
	int data;

	MEC17XX_TMR16_STS(1) = 1; /* Ack the interrupt */
	if ((1 << 1) & MEC17XX_INT_RESULT(MEC17XX_TMR16_GIRQ)) {
		data = port_80_read();

		if (data != PORT_80_IGNORE) {
			idle_count = 0;
			port_80_write(data);
		}
	}

	if (++idle_count >= INTERRUPT_DISABLE_IDLE_COUNT)
		port_80_interrupt_disable();
}
DECLARE_IRQ(MEC17XX_IRQ_TIMER16_1, port_80_interrupt, 2);

#else

/*
 * TODO - implement hooks above if necessary
 */

/*
 * Interrupt fires when number of bytes written
 * to eSPI/LPC I/O 80h-81h exceeds Por80_0 FIFO level
 * Issues:
 * 1. eSPI will not break 16-bit I/O into two 8-bit writes
 *    as LPC does. This means Port80 hardware will capture
 *    only bits[7:0] of data.
 * 2. If Host performs write of 16-bit code as consecutive
 *    byte writes the Port80 hardware will capture both but
 *    we do not know the order it was written.
 * 3. If Host sometimes writes one byte code to I/O 80h and
 *    sometimes two byte code to I/O 80h/81h how do we determine
 *    what to do?
 *
 * An alternative is to document Host must write 16-bit codes
 * to I/O 80h and 90h.  LSB to 0x80 and MSB to 0x90.
 *
 * Set interrupt priority to 3 allowing this ISR to pre-empt other
 * large/slow handlers.
 */
void port_80_interrupt(void)
{
	int d;

	while (MEC17XX_P80_STS(0) & MEC17XX_P80_STS_NOT_EMPTY) {
		/* this masks off time stamp d = port_80_read(); */
		d = MEC17XX_P80_CAP(0);	/* b[7:0] = data, b[31:8] = timestamp */
		TRACE1(73, P80, 0, "Port80h = 0x%02x",(d & 0xff));
		port_80_write(d & 0xff);
	}

	MEC17XX_INT_SOURCE(MEC17XX_P80_GIRQ) = MEC17XX_P80_GIRQ_BIT(0);
}
DECLARE_IRQ(MEC17XX_IRQ_PORT80DBG0, port_80_interrupt, 3);


#endif
