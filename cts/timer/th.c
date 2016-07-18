/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "th_common.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

static enum cts_error_code timer_calibration_test(void)
{
	timestamp_t t0, t1;
	const uint32_t delta_usec = 10;

	gpio_set_flags(GPIO_INPUT_TEST, GPIO_INPUT | GPIO_PULL_UP);

	sync();
	t0 = get_time();
	/* super tight loop */
	while (!gpio_get_level(GPIO_INPUT_TEST))
		;
	t1 = get_time();

	if ((uint32_t)(t1.val - t0.val) < SECOND - delta_usec)
		return CTS_ERROR_FAILURE;
	if (SECOND + delta_usec < (uint32_t)(t1.val - t0.val))
		return CTS_ERROR_FAILURE;

	return CTS_SUCCESS;
}

#include "cts_testlist.h"

void cts_task(void)
{
	enum cts_error_code rc;
	int i;

	uart_flush_output();
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
