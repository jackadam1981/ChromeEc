/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * IR357x driver.
 */

#include "board.h"
#include "console.h"
#include "hooks.h"
#include "i2c.h"
#include "timer.h"
#include "uart.h"
#include "util.h"

/* 8-bit I2C address */
#define IR357x_I2C_ADDR (0x8 << 1)

static uint8_t ir357x_settings[][2] = {
	{0x18, 0x22},{0x19, 0x22},{0x1a, 0x08},{0x1b, 0x10},
	{0x1c, 0x06},{0x1d, 0x21},{0x1e, 0x21},{0x1f, 0x83},
	{0x20, 0x83},{0x21, 0x05},{0x22, 0x00},{0x23, 0x00},
	{0x24, 0x00},{0x25, 0x00},{0x26, 0x00},{0x27, 0x34},
	{0x28, 0x34},{0x29, 0x74},{0x2a, 0x4e},{0x2b, 0xff},
	{0x2c, 0x00},{0x2d, 0x59},{0x2e, 0x10},{0x2f, 0x59},
	{0x30, 0x9a},{0x31, 0x9a},{0x32, 0x25},{0x33, 0x19},
	{0x34, 0xe9},{0x35, 0x40},{0x36, 0x90},{0x37, 0x6d},
	{0x38, 0x75},{0x39, 0x48},{0x3a, 0x24},{0x3b, 0x08},
	{0x3c, 0xc5},{0x3d, 0xa0},{0x3e, 0x80},{0x3f, 0xaa},
	{0x40, 0x50},{0x41, 0x4b},{0x42, 0x02},{0x43, 0x04},
	{0x44, 0x00},{0x45, 0x00},{0x46, 0x00},{0x47, 0x78},
	{0x48, 0x54},{0x49, 0x22},{0x4a, 0x6a},{0x4b, 0x00},
	{0x4c, 0x80},{0x4d, 0x60},{0x4e, 0x60},
	{0x51, 0x00},{0x52, 0x4b},{0x53, 0xaa},
	{0x54, 0xd8},{0x55, 0x46},{0x56, 0x31},{0x57, 0x1a},
	{0x58, 0x12},{0x59, 0x63},{0x5a, 0x40},{0x5b, 0x09},
	{0x5c, 0x02},{0x5d, 0x00},{0x5e, 0xea},{0x5f, 0x00},
	{0x60, 0xb0},{0x61, 0x00},{0x62, 0x00},{0x63, 0x43},
	{0x64, 0x00},{0x65, 0x00},{0x66, 0x00},{0x67, 0x00},
	{0x68, 0x28},{0x69, 0x00},{0x6a, 0x00},{0x6b, 0x00},
	{0x70, 0x80},{0x71, 0x00},{0x72, 0x00},{0x73, 0x00},
	{0x74, 0x00},{0x75, 0xff},{0x76, 0x06},{0x77, 0xff},
	{0x78, 0xff},{0x79, 0x04},{0x7a, 0x00},{0x7b, 0x00},
	{0x7c, 0xa0},{0x7d, 0x10},{0x7e, 0x06},{0x7f, 0x8a},
	{0x80, 0x9a},{0x81, 0x11},{0x82, 0x00},{0x83, 0x00},
	{0xd4, 0x00},{0xd5, 0x00},{0xd6, 0x00},{0xd7, 0x3f},
	{0xd8, 0x00},{0xd9, 0x00},{0xda, 0x00},{0xdb, 0x03},
	{0xdc, 0xc0},{0xdd, 0xe0},{0xde, 0x00},{0xdf, 0x00},
	{0xe0, 0x00},{0xe1, 0x00},{0xe2, 0x00},{0xe3, 0x0a},
	{0xe4, 0x00},{0xe5, 0x07},{0xe6, 0x01},{0xe7, 0x00},
	{0xe8, 0x00},{0xe9, 0x00},{0xea, 0x00},{0xeb, 0x00},
	{0xec, 0x00},{0xed, 0x00},{0xee, 0x00},{0xef, 0x00},
	{0xf0, 0x00},{0xf1, 0x00},{0xf2, 0x00},{0xf3, 0x00},
	{0xf4, 0x00},{0xf5, 0x00},{0xf6, 0x00},{0xf7, 0x00},
};

static uint8_t ir357x_read(uint8_t reg)
{
	int res;
	int val;

	res = i2c_read8(I2C_PORT_REGULATOR, IR357x_I2C_ADDR, reg, &val);
	if (res)
		return 0xee;

	return val;
}

static void ir357x_write(uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_REGULATOR, IR357x_I2C_ADDR, reg, val);
	if (res)
		uart_printf("I2C write failed\n");
}

void ir357x_prog(void)
{
	int i;

	for (i = 0; i < sizeof(ir357x_settings) / sizeof(ir357x_settings[0]);
	     i++)
		ir357x_write(ir357x_settings[i][0], ir357x_settings[i][1]);
}

void ir357x_dump(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (!(i & 0xf)) {
			uart_printf("\n%02x: ", i);
			cflush();
		}
		uart_printf("%02x ", ir357x_read(i));
	}
	uart_printf("\n");
}

int ir357x_check(void)
{
	int i;
	uint8_t val;
	int diff = 0;

	for (i = 0; i < sizeof(ir357x_settings) / sizeof(ir357x_settings[0]);
	     i++)
		if ((val = ir357x_read(ir357x_settings[i][0])) != ir357x_settings[i][1]) {
			uart_printf("DIFF reg 0x%02x %02x->%02x\n", ir357x_settings[i][0], ir357x_settings[i][1], val);
			cflush();
			diff++;
		}
	return !!diff;
}

static void ir357x_mtp(void)
{
	uint8_t user_ptr = ir357x_read(0xa7) & 0xf;
	uint8_t manufact_ptr = ir357x_read(0xa6) & 0x7;
	uint8_t user_next = (user_ptr + 1) & 0xf;
	uint8_t manufact_next = (manufact_ptr + 1) & 0x7;
	int user_remain = 9 - (int)user_next;
	int manufact_remain = 3 - (int)manufact_next;
	timestamp_t deadline;

	uart_printf("user ptr %d next %d remain %d\n",
	       user_ptr, user_next, user_remain);
	uart_printf("manufacturer ptr %d next %d remain %d\n",
	       manufact_ptr, manufact_next, manufact_remain);

	if (user_remain < 0 || manufact_remain < 0) {
		uart_printf("no more MTP cycle\n");
		return;
	}
	/* Unlock protected registers */
	ir357x_write(0xe4, 0); /* Vmax reg */
#if 0
	ir357x_write(0xe5, 0); /* PMBus and SVID address reg */
#endif
	/* write configuration */
	ir357x_prog();
	/* Enable programming clock */
	ir357x_write(0x71, ir357x_read(0x71) | 4);

	/* User section write / 200ms timeout */
	ir357x_write(0xd0, 0x40 | user_next);
	deadline.val = get_time().val + 200000;
	while ((ir357x_read(0xd0) & 0xe0) && (get_time().val < deadline.val)) ;
	if (ir357x_read(0xd0) & 0xe0) {
		uart_printf("timeout while writing user MTP\n");
		return;
	}

	/* Manufacturer section write / 100ms timeout */
	ir357x_write(0xd0, 0x58 | manufact_next);
	deadline.val = get_time().val + 100000;
	while ((ir357x_read(0xd0) & 0xe0) && (get_time().val < deadline.val)) ;
	if (ir357x_read(0xd0) & 0xe0) {
		uart_printf("timeout while writing manufacturer MTP\n");
		return;
	}
}

static int command_ir357x(int argc, char **argv)
{
	int reg, val;
	char *rem;

	if (1 == argc) { /* dump all registers */
		ir357x_dump();
		return EC_SUCCESS;
	} else if (2 == argc ) {
		if (!strcasecmp(argv[1],"prog")) {
			if (ir357x_check())
				ir357x_mtp ();
		} else if (!strcasecmp(argv[1],"check")) {
			ir357x_check();
		} else { /* read one register */
			reg = strtoi(argv[1], &rem, 16);
			if (*rem) {
				ccprintf("Invalid register: %s\n", argv[1]);
				return EC_ERROR_INVAL;
			}
			ccprintf("reg 0x%02x = 0x%02x\n", reg,
				 ir357x_read(reg));
		}
		return EC_SUCCESS;
	} else if (3 == argc) { /* write one register */
		reg = strtoi(argv[1], &rem, 16);
		if (*rem) {
			ccprintf("Invalid register: %s\n", argv[1]);
			return EC_ERROR_INVAL;
		}
		val = strtoi(argv[2], &rem, 16);
		if (*rem) {
			ccprintf("Invalid value: %s\n", argv[2]);
			return EC_ERROR_INVAL;
		}
		ir357x_write(reg, val);
		return EC_SUCCESS;
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(ir357x, command_ir357x,
			"[prog|check|write]",
			"IR357x core regulator control",
			NULL);

static int ir357x_hot_settings(void)
{
	ccprintf("IR357x BEFORE:\n");
	ir357x_check();
#if 0
	ir357x_prog();
	ccprintf("IR357x AFTER:\n");
	ir357x_check();
#endif

	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ir357x_hot_settings, HOOK_PRIO_DEFAULT);
