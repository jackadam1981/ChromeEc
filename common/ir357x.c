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

/* the current settings are for IR3571 */
#define IR357x_SUPPORTED_CHIP 3571

static uint8_t ir357x_settings[][2] = {
	{0x18, 0x22}, {0x19, 0x22}, {0x1a, 0x08}, {0x1b, 0x10},
	{0x1c, 0x06}, {0x1d, 0x21}, {0x1e, 0x21}, {0x1f, 0x83},
	{0x20, 0x83}, {0x21, 0x00}, {0x22, 0x00}, {0x23, 0x00},
	{0x24, 0x00}, {0x25, 0x00}, {0x26, 0x00}, {0x27, 0x34},
	{0x28, 0x34}, {0x29, 0x74}, {0x2a, 0x4e}, {0x2b, 0xff},
	{0x2c, 0x00}, {0x2d, 0x1e}, {0x2e, 0x19}, {0x2f, 0x60},
	{0x30, 0x9a}, {0x31, 0x9a}, {0x32, 0x25}, {0x33, 0x19},
	{0x34, 0xe9}, {0x35, 0x40}, {0x36, 0x90}, {0x37, 0x6d},
	{0x38, 0x75}, {0x39, 0x48}, {0x3a, 0x24}, {0x3b, 0x08},
	{0x3c, 0xc5}, {0x3d, 0xa0}, {0x3e, 0x80}, {0x3f, 0xaa},
	{0x40, 0x50}, {0x41, 0x4b}, {0x42, 0x02}, {0x43, 0x04},
	{0x44, 0x00}, {0x45, 0x00}, {0x46, 0x00}, {0x47, 0x78},
	{0x48, 0x54}, {0x49, 0x22}, {0x4a, 0x6a}, {0x4b, 0x00},
	{0x4c, 0x80}, {0x4d, 0x60}, {0x4e, 0x60}, {0x4f, 0xff},
	{0x50, 0xff}, {0x51, 0x00}, {0x52, 0x4b}, {0x53, 0xaa},
	{0x54, 0xd8}, {0x55, 0x46}, {0x56, 0x31}, {0x57, 0x1a},
	{0x58, 0x12}, {0x59, 0x63}, {0x5a, 0x40}, {0x5b, 0x09},
	{0x5c, 0x02}, {0x5d, 0x00}, {0x5e, 0xea}, {0x5f, 0x00},
	{0x60, 0xb0}, {0x61, 0x14}, {0x62, 0x00}, {0x63, 0x66},
	{0x64, 0x00}, {0x65, 0x00}, {0x66, 0x00}, {0x67, 0x00},
	{0x68, 0x14}, {0x69, 0x00}, {0x6a, 0x00}, {0x6b, 0x00},
	{0x6c, 0x00}, {0x6d, 0x00}, {0x6e, 0x00}, {0x6f, 0x00},
	{0x70, 0x80}, {0x71, 0x00}, {0x72, 0x00}, {0x73, 0x00},
	{0x74, 0x00}, {0x75, 0xff}, {0x76, 0x06}, {0x77, 0xfb},
	{0x78, 0xfb}, {0x79, 0x04}, {0x7a, 0x00}, {0x7b, 0x01},
	{0x7c, 0xa0}, {0x7d, 0x10}, {0x7e, 0x20}, {0x7f, 0x8a},
	{0x80, 0x1b}, {0x81, 0x11}, {0x82, 0x00}, {0x83, 0x00},
	{0x84, 0x00}, {0x85, 0x00}, {0x86, 0x00}, {0x87, 0x00},
	{0x88, 0x00}, {0x89, 0x00}, {0x8a, 0x00}, {0x8b, 0x00},
	{0x8c, 0x00}, {0x8d, 0x00}, {0x8e, 0x00}, {0x8f, 0x00},
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
		ccprintf("I2C write failed\n");
}

int ir357x_get_version(void)
{
	/* IR3571 on Link EVT+ */
	if ((ir357x_read(0xfc) == 'I') && (ir357x_read(0xfd) == 'R') &&
	    ((ir357x_read(0x0a) & 0xe) == 0))
		return 3571;

	/* IR3570A on Link Proto 0/1 */
	if ((ir357x_read(0x92) == 'C') && (ir357x_read(0xcd) == 0x24))
		return 3570;

	/* Unknown and unsupported chip */
	return -1;
}

void ir357x_prog(void)
{
	int i;
	int version = ir357x_get_version();

	if (version != IR357x_SUPPORTED_CHIP) {
		ccprintf("Unsupported chip IR %d. Skip writing settings !\n",
			 version);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(ir357x_settings); i++)
		ir357x_write(ir357x_settings[i][0], ir357x_settings[i][1]);

	ccprintf("IR%d registers UPDATED.\n", version);
}

void ir357x_dump(void)
{
	int i;

	for (i = 0; i < 256; i++) {
		if (!(i & 0xf)) {
			ccprintf("\n%02x: ", i);
			cflush();
		}
		ccprintf("%02x ", ir357x_read(i));
	}
	ccprintf("\n");
}

int ir357x_check(void)
{
	int i;
	uint8_t val;
	int diff = 0;

	for (i = 0; i < ARRAY_SIZE(ir357x_settings); i++)
		val = ir357x_read(ir357x_settings[i][0]);
		if (val != ir357x_settings[i][1]) {
			ccprintf("DIFF reg 0x%02x %02x->%02x\n",
				 ir357x_settings[i][0],
				 ir357x_settings[i][1], val);
			cflush();
			diff++;
		}
	return !!diff;
}

static void ir357x_telemetry(void)
{
	uint8_t vin_supply = ir357x_read(0x94);
	uint8_t v3_supply = ir357x_read(0x95);
	uint8_t vaux_supply = ir357x_read(0x96);
	uint8_t l1_vout = ir357x_read(0x97);
	uint8_t l2_vout = ir357x_read(0x98);
	uint8_t l1_iout = ir357x_read(0x99);
	uint8_t l2_iout = ir357x_read(0x9a);
	uint8_t temp1 = ir357x_read(0x9b);
	uint8_t temp2 = ir357x_read(0x9c);
	uint8_t l1_iin = ir357x_read(0xae);
	uint8_t l2_iin = ir357x_read(0xaf);

	if (temp1 == 0xee && temp2 == 0xee) {
		ccprintf("cannot communicate with IR chip. not powered ?\n");
		return;
	}

	ccprintf("Voltages:\n");
	ccprintf("supply Vin %5d mV   V33 %5d mV Vaux %5d mV\n",
		 (unsigned)vin_supply * 125, (unsigned)v3_supply * 256 / 5,
		 (unsigned)vaux_supply * 1024 * 14 / 5);
	ccprintf("Vout loop1 %5d mV loop2 %5d mV\n",
		 (unsigned)l1_vout * 1000 / 128,
		 (unsigned)l2_vout * 1000 / 128);
	ccprintf("Currents:\n");
	ccprintf("Iout loop1 %5d mA loop2 %5d mA\n",
		 (unsigned)l1_iout * 2000, (unsigned)l2_iout * 500);
	ccprintf("Iin  loop1 %5d mA loop2 %5d mA\n",
		 (unsigned)l1_iin * 125, (unsigned)l2_iin * 125 / 2);
	ccprintf("Power:\n");
	ccprintf("out  loop1 %5d mW loop2 %5d mW\n",
		 (unsigned)l1_iout * 2000 * (unsigned)l1_vout / 128,
		 (unsigned)l2_iout * 500 * (unsigned)l2_vout / 128);
	ccprintf("in   loop1 %5d mW loop2 %5d mW\n",
		 (unsigned)l1_iin * 125 * (unsigned)vin_supply * 125 / 1000,
		 (unsigned)l2_iin * 125 * (unsigned)vin_supply * 125 / 2000);
	ccprintf("Temperatures:\n");
	ccprintf("loop1 %3d C loop2 %3d C\n", temp1, temp2);
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
	int version = ir357x_get_version();

	if (version != IR357x_SUPPORTED_CHIP) {
		ccprintf("Unsupported chip IR %d. Skip programming !\n",
			 version);
		return;
	}

	ccprintf("user ptr %d next %d remain %d\n",
	       user_ptr, user_next, user_remain);
	ccprintf("manufacturer ptr %d next %d remain %d\n",
	       manufact_ptr, manufact_next, manufact_remain);

	if (user_remain < 0 || manufact_remain < 0) {
		ccprintf("no more MTP cycle\n");
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
	while ((ir357x_read(0xd0) & 0xe0) && (get_time().val < deadline.val))
		;
	if (ir357x_read(0xd0) & 0xe0) {
		ccprintf("timeout while writing user MTP\n");
		return;
	}

	/* Manufacturer section write / 100ms timeout */
	ir357x_write(0xd0, 0x58 | manufact_next);
	deadline.val = get_time().val + 100000;
	while ((ir357x_read(0xd0) & 0xe0) && (get_time().val < deadline.val))
		;
	if (ir357x_read(0xd0) & 0xe0) {
		ccprintf("timeout while writing manufacturer MTP\n");
		return;
	}
}

static int command_ir357x(int argc, char **argv)
{
	int reg, val;
	char *rem;

	if (1 == argc) {
		ir357x_telemetry();
		return EC_SUCCESS;
	} else if (2 == argc) {
		if (!strcasecmp(argv[1], "prog")) {
			if (ir357x_check())
				ir357x_mtp();
		} else if (!strcasecmp(argv[1], "check")) {
			ir357x_check();
		} else if (!strcasecmp(argv[1], "dump")) {
			/* dump all registers */
			ir357x_dump();
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
			"[prog|check|dump]",
			"IR357x core regulator control",
			NULL);

static int ir357x_hot_settings(void)
{
	/* dynamically apply settings to workaround issue */
	ir357x_prog();

	return EC_SUCCESS;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, ir357x_hot_settings, HOOK_PRIO_DEFAULT);
