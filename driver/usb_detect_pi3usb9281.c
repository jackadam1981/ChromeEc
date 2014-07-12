/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB9281 USB charger detection driver.
 */

#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "pi3usb9281.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* 8-bit I2C address */
#define PI3USB9281_I2C_ADDR (0x25 << 1)

#ifdef CONFIG_PI3USB9281_MUX_GPIO
#define PI3USB9281_COUNT 2
static inline void select_chip(int chip_idx)
{
	gpio_set_level(CONFIG_PI3USB9281_MUX_GPIO, chip_idx);
}
#else
#define PI3USB9281_COUNT 1
#define select_chip(x)
#endif

static int saved_interrupts[PI3USB9281_COUNT];

uint8_t pi3usb9281_read(int chip_idx, uint8_t reg)
{
	int res, val;

	select_chip(chip_idx);
	res = i2c_read8(I2C_PORT_MASTER, PI3USB9281_I2C_ADDR, reg, &val);
	if (res)
		return 0xee;

	return val;
}

int pi3usb9281_write(int chip_idx, uint8_t reg, uint8_t val)
{
	int res;

	select_chip(chip_idx);
	res = i2c_write8(I2C_PORT_MASTER, PI3USB9281_I2C_ADDR, reg, val);
	if (res)
		CPRINTS("PI3USB9281 I2C write failed");
	return res;
}

int pi3usb9281_enable_interrupts(int chip_idx)
{
	int ctrl = pi3usb9281_read(chip_idx, PI3USB9281_REG_CONTROL);
	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;
	return pi3usb9281_write(chip_idx, PI3USB9281_REG_CONTROL, ctrl & 0xfe);
}

int pi3usb9281_disable_interrupts(int chip_idx)
{
	int ctrl = pi3usb9281_read(chip_idx, PI3USB9281_REG_CONTROL);
	int rv;

	if (ctrl == 0xee)
		return EC_ERROR_UNKNOWN;
	rv = pi3usb9281_write(chip_idx, PI3USB9281_REG_CONTROL,
			      (ctrl | 0x1) & 0x15);
	pi3usb9281_get_interrupts(chip_idx);
	return rv;
}

int pi3usb9281_set_interrupt_mask(int chip_idx, uint8_t mask)
{
	return pi3usb9281_write(chip_idx, PI3USB9281_REG_INT_MASK, mask);
}

int pi3usb9281_get_interrupts(int chip_idx)
{
	int ret = pi3usb9281_peek_interrupts(chip_idx);
	saved_interrupts[chip_idx] = 0;
	return ret;
}

int pi3usb9281_peek_interrupts(int chip_idx)
{
	saved_interrupts[chip_idx] |= pi3usb9281_read(chip_idx,
						      PI3USB9281_REG_INT);
	return saved_interrupts[chip_idx];
}

int pi3usb9281_get_device_type(int chip_idx)
{
	uint8_t chg, dev_type;

	chg = pi3usb9281_read(chip_idx, PI3USB9281_REG_CHG_STATUS) & 0x1f;
	dev_type = pi3usb9281_read(chip_idx, PI3USB9281_REG_DEV_TYPE);

	return (chg << 8) | dev_type;
}

int pi3usb9281_get_vbus(int chip_idx)
{
	int vbus = pi3usb9281_read(chip_idx, PI3USB9281_REG_VBUS);
	if (vbus == 0xee)
		return -1;
	return (vbus & 0x2) ? 1 : 0;
}

void pi3usb9281_reset(int chip_idx)
{
	pi3usb9281_write(chip_idx, PI3USB9281_REG_RESET, 0x1);
}

static void pi3usb9281_init(void)
{
	uint8_t dev_id;
	int i;

	for (i = 0; i < PI3USB9281_COUNT; ++i) {
		dev_id = pi3usb9281_read(i, PI3USB9281_REG_DEV_ID);

		if (dev_id != 0x10)
			CPRINTS("PI3USB9281[%d] invalid ID 0x%02x", i, dev_id);
	}
}
DECLARE_HOOK(HOOK_INIT, pi3usb9281_init, HOOK_PRIO_LAST);
