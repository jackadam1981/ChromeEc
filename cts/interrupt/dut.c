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

static int in_interrupt;

void cts_irq(enum gpio_signal signal)
{
	/* test some APIs */
	in_interrupt = in_interrupt_context();

	/* Wake up the CTS task */
	task_wake(TASK_ID_CTS);
}

enum cts_rc interrupt_test(void)
{
	uint32_t event;

	gpio_enable_interrupt(GPIO_CTS_IRQ);
	interrupt_enable();
	in_interrupt = 0;

	sync();
	/* Sleep and wait for interrupt. This shouldn't time out. */
	event = task_wait_event(CTS_INTERRUPT_TIMEOUT_US);
	if (event != TASK_EVENT_WAKE) {
		CPRINTS("Woke up by 0x%08x", event);
		return CTS_RC_FAILURE;
	}
	if (!in_interrupt) {
		CPRINTS("Interrupt context not detected");
		return CTS_RC_FAILURE;
	}

	return CTS_RC_SUCCESS;
}

enum cts_rc interrupt_disable_test(void)
{
	uint32_t event;

	gpio_enable_interrupt(GPIO_CTS_IRQ);
	task_disable_irq(CTS_IRQ_NUMBER);

	sync();
	/* Sleep and wait for interrupt. This should time out. */
	event = task_wait_event(CTS_INTERRUPT_TIMEOUT_US);
	if (event != TASK_EVENT_TIMER) {
		CPRINTS("Woke up by 0x%08x", event);
		return CTS_RC_FAILURE;
	}

	task_enable_irq(CTS_IRQ_NUMBER);

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
