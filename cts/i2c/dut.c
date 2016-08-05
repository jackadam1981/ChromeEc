/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "watchdog.h"
#include "uart.h"
#include "timer.h"
#include "watchdog.h"
#include "dut_common.h"
#include "cts_common.h"
#include "registers.h"
#include "i2c.h"

enum cts_rc send_test(void)
{
	int port = i2c_ports[0].port;

	sleep(1);
	i2c_write8(port, 0x3c, 0x00, 0xAA);
	return CTS_RC_SUCCESS;
}

#include "cts_testlist.h"

void cts_task(void)
{
	enum cts_rc result;
	int i;

	uart_flush_output();
	for (i = 0; i < CTS_TEST_ID_COUNT; i++) {
		sync();
		result = tests[i].run();
		CPRINTF("\n%s %d\n", tests[i].name, result);
		uart_flush_output();
	}

	CPRINTS("GPIO test suite finished");
	uart_flush_output();
	while (1) {
		watchdog_reload();
		sleep(1);
	}
}
