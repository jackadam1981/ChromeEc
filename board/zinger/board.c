/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Tiny charger configuration */

#include "common.h"
#include "debug.h"
#include "irq_handler.h"
#include "registers.h"
#include "usb_pd.h"
#include "util.h"

/* External interrupt EXTINT7 for external comparator on PA7 */
void IRQ_HANDLER(STM32_IRQ_EXTI0_1)(void)
{
	debug_printf("!\n");
	/* send PD_EVENT_RX */
	last_event = PD_EVENT_RX;
}

extern void pd_task(void);

int main(void)
{
	hardware_init();
	debug_printf("Power supply started ...\n");

	/* background loop for PD events */
	pd_task();

	while (1)
		;
}
