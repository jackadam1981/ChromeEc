/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "th_common.h"
#include "gpio.h"
#include "timer.h"
#include "watchdog.h"

static enum cts_rc interrupt_test(void)
{
	gpio_set_flags(GPIO_OUTPUT_TEST, GPIO_ODR_HIGH);
	gpio_set_level(GPIO_OUTPUT_TEST, 1);

	sync();
	/* Wait a little for DUT to go into sleep */
	usleep(CTS_INTERRUPT_TIMEOUT_US / 2);
	/* Wake up DUT by interrupt */
	gpio_set_level(GPIO_OUTPUT_TEST, 0);

	return CTS_RC_SUCCESS;
}

static enum cts_rc interrupt_disable_test(void)
{
	gpio_set_flags(GPIO_OUTPUT_TEST, GPIO_ODR_HIGH);
	gpio_set_level(GPIO_OUTPUT_TEST, 1);

	sync();
	/* Wait a little for DUT to go into sleep */
	usleep(CTS_INTERRUPT_TIMEOUT_US / 2);
	/* Wake up DUT by interrupt */
	gpio_set_level(GPIO_OUTPUT_TEST, 0);

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
