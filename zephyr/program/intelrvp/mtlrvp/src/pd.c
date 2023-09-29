/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"

void board_dc_jack_interrupt(enum gpio_signal signal)
{
}


static int test_i2c(int argc, const char **argv)
{
	uint8_t w_buf[] = {0x1c, 0x10};
	uint8_t r_buf[10] = { 0 };
	int rv;

	rv = i2c_xfer(0, 0x08, w_buf, 2, r_buf, 4);
	cflush();
	ccprintf("rv=%d, %x %x %x %x %x\n", rv, r_buf[0], r_buf[1], r_buf[2], r_buf[3], r_buf[4]);
	cflush();

	return 0;
}
DECLARE_CONSOLE_COMMAND(test, test_i2c, "<repeat_count> <sleep_ms>",
			"Print battery info");
