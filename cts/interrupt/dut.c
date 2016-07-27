/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "gpio.h"
#include "registers.h"
#include "task.h"
#include "dut_common.h"
#include "timer.h"
#include "watchdog.h"

/*
 * Interrupt handler
 *
 * DUT is supposed to trigger an interrupt when it's done counting down,
 * causing this function to be invoked.
 */
void cts_irq(enum gpio_signal signal)
{
	/* Wake up the CTS task */
	task_set_event(TASK_ID_CTS, TASK_EVENT_WAKE, 0);
}

static enum cts_rc interrupt_test(void)
{
	gpio_enable_interrupt(GPIO_CTS_IRQ);
	interrupt_enable();

	sync();
	/* Sleep and wait for interrupt */
	task_wait_event(-1);

	return CTS_RC_SUCCESS;
}

#include "cts_testlist.h"

void cts_task(void)
{
	enum cts_rc rc;
	int i;

	for (i = 0; i < CTS_TEST_ID_COUNT; i++) {
		sync();
		rc = tests[i].run();
		CPRINTF("\n%s %d\n", tests[i].name, rc);
		cflush();
	}

	CPRINTS("Interrupt test suite finished");
	cflush();

	while (1) {
		watchdog_reload();
		sleep(1);
	}
}
