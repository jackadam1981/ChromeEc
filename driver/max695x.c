/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * MAX6958/MAX6959 7-Segment LED Display Driver
 */

#include "common.h"
#include "console.h"
#include "driver/max695x.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"

static inline int max695x_i2c_write8(uint8_t offset, uint8_t data)
{
	return i2c_write8(I2C_PORT_PORT80, PORT80_I2C_ADDR,
			   offset, (int)data);
}

static inline int max695x_i2c_write16(uint8_t offset, uint8_t *data)
{
	return i2c_write16(I2C_PORT_PORT80, PORT80_I2C_ADDR,
			   offset, *(int *)data);
}

static inline int max695x_i2c_write32(uint8_t offset, uint8_t *data)
{
	/*
	 * The address pointer stored in the MAX695x increments after
	 * each data byte is written unless the address equals 01111111
	 */
	return i2c_write32(I2C_PORT_PORT80, PORT80_I2C_ADDR,
			   offset, *(int *)data);
}

int port80_display_write(int data)
{
	int i;
	uint8_t buf[2];

	/*
	 * In hexadecimal code-decode mode, the decoder looks only at
	 * the lower nibble of the data in the digit register (D3–D0),
	 * disregarding bits D7–D4. Hence, preparing the hexadecimal
	 * buffer to be sent.
	 */
	for (i = 0; i < 2; i++)
		buf[1 - i] = (data >> (i * 4)) & 0xF;
	return max695x_i2c_write16(MAX695X_DIGIT0_ADDR, buf);
}

/**
 * Initialise MAX656x 7-segment display.
 */
void max695x_init(void)
{
	uint8_t buf[4] = {
		[0] = MAX695X_DECODE_MODE_HEX_DECODE,
		[1] = MAX695X_INTENSITY_MEDIUM,
		/* Port80 data is always less than 0x100 */
		[2] = MAX695X_SCAN_LIMIT_2,
		[3] = MAX695X_CONFIG_OPR_NORMAL
	};
	max695x_i2c_write32(MAX695X_REG_DECODE_MODE, buf);
}
DECLARE_HOOK(HOOK_INIT, max695x_init, HOOK_PRIO_DEFAULT);

void max695x_shutdown(void)
{
	max695x_i2c_write8(MAX695X_REG_CONFIG,
			   MAX695X_CONFIG_OPR_SHUTDOWN);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, max695x_shutdown, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CMD_PORT80_DISPLAY
static int console_command_max695x_write(int argc, char **argv)
{
	char *e;
	int rv,  val;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	/* Get value to be written to the seven segment display*/
	val = strtoi(argv[1], &e, 0);
	if (*e || val < 0)
		return EC_ERROR_PARAM1;

	rv = port80_display_write(val);

	return rv;
}
DECLARE_CONSOLE_COMMAND(seg, console_command_max695x_write,
			"<val>",
			"Write to 7 segment display in hex");
#endif
