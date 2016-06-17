/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "clock.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "rbox.h"
#include "registers.h"
#include "task.h"
#include "timer.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_RBOX, outstr)
#define CPRINTS(format, args...) cprints(CC_RBOX, format, ## args)

#ifdef GC_RBOX_ENABLE_INT
const int num_interrupts = 15;

static const char * const interrupt_descs[] = {
	"AC attached", "AC detached", "Entering RW", "Leaving RW",
	"PWRB released", "PWRB pressed", "KEY0 released", "KEY0 pressed",
	"KEY1 released", "KEY1 pressed", "EC RST rising", "EC RST falling",
	"COMBO0", "COMBO1", "COMBO2"};

static void rbox_int_handler(void)
{
	int state = GREAD(RBOX, INT_STATE);
	int i;

	for (i = 0; i < num_interrupts; i++)
		if (state & 1 << i)
			CPRINTS("%d %s", i, interrupt_descs[i]);

	/* Clear interrupt */
	GWRITE(RBOX, INT_STATE, GREAD(RBOX, INT_STATE));
	GWRITE(RBOX, INT_TEST, 0);
}

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_FED_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_AC_PRESENT_RED_INT, rbox_int_handler, 1);

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_PWRB_IN_FED_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_PWRB_IN_RED_INT, rbox_int_handler, 1);

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_KEY0_IN_RED_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_KEY0_IN_FED_INT, rbox_int_handler, 1);

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_KEY1_IN_RED_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_KEY1_IN_FED_INT, rbox_int_handler, 1);

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_EC_RST_RED_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_EC_RST_FED_INT, rbox_int_handler, 1);

DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_BUTTON_COMBO0_RDY_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_BUTTON_COMBO1_RDY_INT, rbox_int_handler, 1);
DECLARE_IRQ(GC_IRQNUM_RBOX0_INTR_BUTTON_COMBO2_RDY_INT, rbox_int_handler, 1);

static void enable_interrupts(void)
{
	int i;
	/* Enable All interrupts */
	GWRITE(RBOX, INT_ENABLE, 0x7fff);

	for (i = GC_IRQNUM_RBOX0_INTR_AC_PRESENT_FED_INT;
		i <= GC_IRQNUM_RBOX0_INTR_PWRB_IN_RED_INT; i++)
		task_enable_irq(i);
}

static int command_rbox_test(int argc, char **argv)
{
	char *e;
	int i;

	if (argc > 1) {
		i = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;
		GWRITE(RBOX, INT_TEST, 1 << i);
	} else {
		for (i = 0; i < num_interrupts; i++) {
			GWRITE(RBOX, INT_TEST, 1 << i);
			usleep(1);
		}
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(rboxtest, command_rbox_test,
	"int",
	"number between 0 and 14 representing the interrupt",
	NULL);
#endif

void rbox_init(void)
{
	/* Enable RBOX */
	clock_enable_module(MODULE_RBOX, 1);

	/* Clear existing interrupts */
	GWRITE(RBOX, WAKEUP_CLEAR, 1);
	GWRITE(RBOX, WAKEUP_CLEAR, 0);
	GWRITE(RBOX, INT_STATE, 0x7fff);

#ifdef GC_RBOX_ENABLE_INT
	enable_interrupts();
#endif
}
DECLARE_HOOK(HOOK_INIT, rbox_init, HOOK_PRIO_DEFAULT - 1);
