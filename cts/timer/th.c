/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "registers.h"
#include "task.h"
#include "th_common.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

/*
 * Interrupt handler
 *
 * DUT is supposed to trigger an interrupt when it's done counting down,
 * causing this function to be invoked.
 */
void cts_notify(enum gpio_signal signal)
{
	/* Wake up the CTS task */
	task_set_event(TASK_ID_CTS, TASK_EVENT_WAKE, 0);
}

static enum cts_rc timer_calibration_test(void)
{
	/* Error margin: +/-2 msec (0.2% for one second) */
	const uint32_t margin = 2 * MSEC;
	timestamp_t t0, t1;
	uint32_t delta;

	gpio_enable_interrupt(GPIO_CTS_NOTIFY);
	interrupt_enable();

	sync();
	t0 = get_time();
	/* Wait for interrupt */
	task_wait_event(-1);
	t1 = get_time();

	delta = (uint32_t)(t1.val - t0.val);
	CPRINTS("delta=%d", delta);
	if (delta < SECOND - margin)
		return CTS_RC_FAILURE;
	if (SECOND + margin < delta)
		return CTS_RC_FAILURE;

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
		uart_flush_output();
	}

	CPRINTS("Timer test suite finished");
	uart_flush_output();

	while (1) {
		watchdog_reload();
		sleep(1);
	}
}
