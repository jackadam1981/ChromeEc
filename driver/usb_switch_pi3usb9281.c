/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB3281 USB port switch driver.
 */

#include "console.h"
#include "ec_commands.h"
#include "hooks.h"
#include "host_command.h"
#include "i2c.h"
#include "system.h"
#include "timer.h"
#include "pi3usb9281.h"
#include "uart.h"
#include "util.h"

 /* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* 8-bit I2C address */
#define PI3USB9281_I2C_ADDR (0x25 << 1)

/* Delay values */
#define PI3USB9281_SW_RESET_DELAY 20

static int saved_interrupts;

uint8_t pi3usb9281_read(uint8_t reg)
{
	int res;
	int val;

	res = i2c_read8(I2C_PORT_MASTER, PI3USB9281_I2C_ADDR, reg, &val);
	if (res)
		return 0xee;

	return val;
}

int pi3usb9281_write(uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_MASTER, PI3USB9281_I2C_ADDR, reg, val);
	if (res)
		CPRINTS("PI3USB9281 I2C write failed");
	return res;
}

int pi3usb9281_enable_interrupts(void)
{
	int ctrl = pi3usb9281_read(PI3USB9281_REG_CONTROL);
	return pi3usb9281_write(PI3USB9281_REG_CONTROL, ctrl & 0x14);
}

int pi3usb9281_disable_interrupts(void)
{
	int ctrl;
	int rv;

	ctrl = pi3usb9281_read(PI3USB9281_REG_CONTROL);
	if (ctrl == 0xee)
		return ctrl;

	rv = pi3usb9281_write(PI3USB9281_REG_CONTROL,
		ctrl | PI3USB9281_CTRL_INT_MASK);
	pi3usb9281_get_interrupts();
	return rv;
}

int pi3usb9281_set_interrupt_mask(uint8_t mask)
{
	return pi3usb9281_write(PI3USB9281_REG_INT_MASK, ~mask);
}

int pi3usb9281_get_interrupts(void)
{
	int ret = pi3usb9281_peek_interrupts();
	saved_interrupts = 0;
	return ret;
}

int pi3usb9281_peek_interrupts(void)
{
	saved_interrupts |= pi3usb9281_read(PI3USB9281_REG_INT);
	return saved_interrupts;
}

int pi3usb9281_get_device_type(void)
{
	return pi3usb9281_read(PI3USB9281_REG_DEV_TYPE);
}

int pi3usb9281_get_charger_status(void)
{
	return pi3usb9281_read(PI3USB9281_REG_CHG_STATUS);
}

int pi3usb9281_reset(void)
{
	int rv = pi3usb9281_write(PI3USB9281_REG_RESET, 0x1);

	if (!rv)
		/* Reset takes ~15ms. Wait for 20ms to be safe. */
		msleep(PI3USB9281_SW_RESET_DELAY);

	return rv;
}

int pi3usb9281_set_switch_manual(int val)
{
	int ctrl;
	int rv;

	ctrl = pi3usb9281_read(PI3USB9281_REG_CONTROL);
	if (ctrl == 0xee)
		return ctrl;

	if (val)
		rv = pi3usb9281_write(PI3USB9281_REG_CONTROL,
			ctrl & ~PI3USB9281_CTRL_AUTO);
	else
		rv = pi3usb9281_write(PI3USB9281_REG_CONTROL,
			ctrl | PI3USB9281_CTRL_AUTO);

	return rv;
}

int pi3usb9281_set_pins(uint8_t val)
{
	return pi3usb9281_write(PI3USB9281_REG_MANUAL, val);
}

static void pi3usb9281_dump(void)
{
	uint8_t ctrl = pi3usb9281_read(PI3USB9281_REG_CONTROL);

	if (ctrl & PI3USB9281_CTRL_AUTO)
		ccprintf("Auto: %02x\n",
			pi3usb9281_read(PI3USB9281_REG_DEV_TYPE));
	else
		ccprintf("Manual: %02x\n",
			pi3usb9281_read(PI3USB9281_REG_MANUAL));
}

/*****************************************************************************/
/* Console commands */

static int command_usbmux(int argc, char **argv)
{
	int val;
	char *e;

	if (1 == argc) { /* dump all registers */
		pi3usb9281_dump();
		return EC_SUCCESS;
	} else if (2 == argc) {
		val = strtoi(argv[1], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1;

		return pi3usb9281_set_switch_manual(val);
	}

	return EC_ERROR_INVAL;
}
DECLARE_CONSOLE_COMMAND(usbmux, command_usbmux,
	"[0|1]",
	"PI3USB9281 USB mux control",
	NULL);
