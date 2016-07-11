/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "dut_common.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

static enum cts_error_code timer_calibration_test(void)
{
	gpio_set_flags(GPIO_OUTPUT_TEST, GPIO_ODR_LOW);

	sync();
	usleep(SECOND);
	gpio_set_level(GPIO_OUTPUT_TEST, 1);

	return CTS_ERROR_UNKNOWN;
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
