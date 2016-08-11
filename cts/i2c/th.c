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
#include "i2c_const.h"

uint8_t in_mailbox[CTS_I2C_MAILBOX_SIZE];
/* First byte holds size in bytes to send */
uint8_t out_mailbox[CTS_I2C_MAILBOX_SIZE];
uint8_t in_ready;

void get_response(void)
{
	switch(in_mailbox[0]) {
		case READ_8_OFFSET:
			out_mailbox[0] = 1;
			out_mailbox[1] = READ_8_DATA;
			break;
		case READ_16_OFFSET:
			out_mailbox[0] = 2;
			out_mailbox[1] = READ_16_DATA & 0xFF;
			out_mailbox[2] = (READ_16_DATA >> 8) & 0xFF;
			break;
		case READ_32_OFFSET:
			out_mailbox[0] = 4;
			out_mailbox[1] = READ_32_DATA & 0xFF;
			out_mailbox[2] = (READ_32_DATA >> 8) & 0xFF;
			out_mailbox[3] = (READ_32_DATA >> 16) & 0xFF;
			out_mailbox[4] = (READ_32_DATA >> 24) & 0xFF;
			break;
		default:
			out_mailbox[0] = 4;
			out_mailbox[1] = CTS_I2C_FAILURE & 0xFF;
			out_mailbox[2] = (CTS_I2C_FAILURE >> 8) & 0xFF;
			out_mailbox[3] = (CTS_I2C_FAILURE >> 16) & 0xFF;
			out_mailbox[4] = (CTS_I2C_FAILURE >> 24) & 0xFF;
	}
}

/* Return 0 when ready_flag gets set, or 1 if timeout
 * Param: ms_timeout is how many milliseconds you want to
 * wait (minimum) before timing out
 */
int wait_for_in_flag(uint32_t ms_timeout) {
	uint64_t us_delta = 0;
	uint64_t start_time;

	start_time = get_time().val;

	while (us_delta / 1000 < ms_timeout) {
		if (in_ready)
			return 0;
		msleep(5);
		watchdog_reload();
		us_delta = get_time().val - start_time;
	}
	return 1;
}

void clear_mail(void) {
	int i;

	for (i = 0; i < CTS_I2C_MAILBOX_SIZE; i++) {
		in_mailbox[i] = 0;
		out_mailbox[i] = 0;
	}

	in_ready = 0;
}

void print_in_mailbox(void) {
	int i;

	CPRINTF("Mailbox holds the value 0x");
	for (i = CTS_I2C_MAILBOX_SIZE-1; i >= 0; i--) {
		CPRINTF("%02X", in_mailbox[i]);
	}
	CPRINTF("\n");
}

enum cts_rc write8_test(void)
{
	if (wait_for_in_flag(100))
		return CTS_RC_FAILURE;
	if (in_mailbox[0] != WRITE_8_OFFSET)
		return CTS_RC_FAILURE;
	if (in_mailbox[1] != WRITE_8_DATA)
		return CTS_RC_FAILURE;
	else
		return CTS_RC_SUCCESS;
}

enum cts_rc write16_test(void)
{
	if (wait_for_in_flag(100))
		return CTS_RC_FAILURE;
	if (in_mailbox[0] != WRITE_16_OFFSET)
		return CTS_RC_FAILURE;
	if (in_mailbox[1] != (WRITE_16_DATA & 0xFF))
		return CTS_RC_FAILURE;
	if (in_mailbox[2] != ((WRITE_16_DATA >> 8) & 0xFF))
		return CTS_RC_FAILURE;
	else
		return CTS_RC_SUCCESS;
}

enum cts_rc write32_test(void)
{
	if (wait_for_in_flag(100))
		return CTS_RC_FAILURE;
	if (in_mailbox[0] != WRITE_32_OFFSET)
		return CTS_RC_FAILURE;
	if (in_mailbox[1] != (WRITE_32_DATA & 0xFF))
		return CTS_RC_FAILURE;
	if (in_mailbox[2] != ((WRITE_32_DATA >> 8) & 0xFF))
		return CTS_RC_FAILURE;
	if (in_mailbox[3] != ((WRITE_32_DATA >> 16) & 0xFF))
		return CTS_RC_FAILURE;
	if (in_mailbox[4] != ((WRITE_32_DATA >> 24) & 0xFF))
		return CTS_RC_FAILURE;
	else
		return CTS_RC_SUCCESS;
}

enum cts_rc read8_test(void)
{
	return CTS_RC_SUCCESS;
}

enum cts_rc read16_test(void)
{
	return CTS_RC_SUCCESS;
}

enum cts_rc read32_test(void)
{
	return CTS_RC_SUCCESS;
}

#include "cts_testlist.h"

void cts_task(void)
{
	enum cts_rc result;
	int i;
	cflush();
	for (i = 0; i < CTS_TEST_ID_COUNT; i++) {
		clear_mail();
		sync();
		result = tests[i].run();
		CPRINTF("\n%s %d\n", tests[i].name, result);
		uart_flush_output();
	}

	CPRINTS("I2C test suite finished");
	uart_flush_output();
	while (1) {
		watchdog_reload();
		sleep(1);
	}
}
