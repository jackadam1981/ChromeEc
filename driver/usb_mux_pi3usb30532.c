/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Pericom PI3USB30532 USB port switch driver.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "pi3usb30532.h"
#include "usb_mux.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

static int pi3usb30532_read(int i2c_addr, uint8_t reg, uint8_t *val)
{
	int read;

	/*
	 * First byte read will be slave address (ignored).
	 * Second byte read will be vendor ID.
	 * Third byte read will be selection control.
	 */
	if (i2c_read16(I2C_PORT_USB_MUX, i2c_addr, 0, &read)) {
		CPUTS("PI3USB30532 I2C read failed");
		return -1;
	}

	if (reg == PI3USB30532_REG_VENDOR)
		*val = read & 0xff;
	else if (reg == PI3USB30532_REG_CONTROL)
		*val = (read >> 8) & 0xff;

	return 0;
}

static int pi3usb30532_write(int i2c_addr, uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_USB_MUX, i2c_addr, 0, val);
	if (res || reg != PI3USB30532_REG_CONTROL)
		CPUTS("PI3USB30532 I2C write failed");

	return res;
}

static int pi3usb30532_reset(int i2c_addr)
{
	return pi3usb30532_write(
		i2c_addr,
		PI3USB30532_REG_CONTROL,
		(PI3USB30532_MODE_POWERDOWN & PI3USB30532_CTRL_MASK) |
		PI3USB30532_CTRL_RSVD);
}

static int pi3usb30532_init(int i2c_addr)
{
	uint8_t val;

	if (pi3usb30532_reset(i2c_addr))
		return -1;
	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_VENDOR, &val) ||
	    val != PI3USB30532_VENDOR_ID)
		return -1;

	return 0;
}

/* Writes control register to set switch mode */
static int pi3usb30532_set_mux(int i2c_addr, mux_state_t mux_state)
{
	uint8_t reg = 0;

	if (mux_state & MUX_USB_ENABLED)
		reg |= PI3USB30532_MODE_USB;
	if (mux_state & MUX_DP_ENABLED)
		reg |= PI3USB30532_MODE_DP;
	if (mux_state & MUX_POLARITY_INVERTED)
		reg |= PI3USB30532_BIT_SWAP;

	return pi3usb30532_write(i2c_addr, PI3USB30532_REG_CONTROL,
				 reg | PI3USB30532_CTRL_RSVD);
}

/* Reads control register and updates mux_state accordingly */
static int pi3usb30532_get_mux(int i2c_addr, mux_state_t *mux_state)
{
	uint8_t reg;

	*mux_state = 0;
	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_CONTROL, &reg))
		return -1;

	if (reg & PI3USB30532_MODE_USB)
		*mux_state |= MUX_USB_ENABLED;
	if (reg & PI3USB30532_MODE_DP)
		*mux_state |= MUX_DP_ENABLED;
	if (reg & PI3USB30532_BIT_SWAP)
		*mux_state |= MUX_POLARITY_INVERTED;

	return 0;
}

const struct usb_mux_driver pi3usb30532_usb_mux_driver = {
	.init = pi3usb30532_init,
	.set = pi3usb30532_set_mux,
	.get = pi3usb30532_get_mux,
};
