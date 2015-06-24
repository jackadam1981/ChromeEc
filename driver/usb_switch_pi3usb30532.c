/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB30532 USB port switch driver.
 */

#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "task.h"
#include "timer.h"
#include "pi3usb30532.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

#define INTERNAL_READ_ERROR(code) (EC_ERROR_INTERNAL_FIRST + code)
#define IS_READ_ERROR(code) ((code) > EC_ERROR_INTERNAL_FIRST)

/* 8-bit I2C address */
static const int pi3usb30532_addrs[] = CONFIG_USB_SWITCH_PI3USB30532_I2C_ADDRS;

static int pi3usb30532_read(int chip_idx, uint8_t reg)
{
	int res, val;
	int addr = pi3usb30532_addrs[chip_idx];

	res = i2c_read8(I2C_PORT_USB_SWITCH, addr, reg, &val);
	if (res)
		return INTERNAL_READ_ERROR(res);

	return val;
}

static int pi3usb30532_write(int chip_idx, uint8_t reg, uint8_t val)
{
	int res;
	int addr = pi3usb30532_addrs[chip_idx];

	res = i2c_write8(I2C_PORT_USB_SWITCH, addr, reg, val);
	if (res)
		CPUTS("PI3USB30532 I2C write failed");

	return res;
}

static int pi3usb30532_reset(int chip_idx)
{
	return pi3usb30532_set_switch(chip_idx, PI3USB30532_MODE_POWERDOWN);
}

void pi3usb30532_init(int chip_idx)
{
	int res, val;
	ASSERT(chip_idx < ARRAY_SIZE(pi3usb30532_addrs));

	res = pi3usb30532_reset(chip_idx);
	if (res)
		CPRINTS("PI3USB30532 [%d] init failed", chip_idx);

	val = pi3usb30532_read(chip_idx, PI3USB30532_REG_VENDOR);
	if (IS_READ_ERROR(val))
		CPRINTS("PI3USB30532 [%d] read failed", chip_idx);
	else if (val != PI3USB30532_VENDOR_ID)
		CPRINTS("PI3USB30532 [%d] invalid ID 0x%02x", chip_idx, val);
}

/* Writes control register to set switch mode */
int pi3usb30532_set_switch(int chip_idx, uint8_t mode)
{
	return pi3usb30532_write(chip_idx, PI3USB30532_REG_CONTROL,
				 (mode & PI3USB30532_CTRL_MASK) |
				 PI3USB30532_CTRL_RSVD);
}
