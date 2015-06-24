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
#include "usb_mux.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

static int pi3usb30532_read(int i2c_addr, uint8_t reg, uint8_t *val)
{
	int read;

	if (i2c_read8(I2C_PORT_USB_SWITCH, i2c_addr, reg, &read))
		return -1;

	*val = read;
	return 0;
}

static int pi3usb30532_write(int i2c_addr, uint8_t reg, uint8_t val)
{
	int res;

	res = i2c_write8(I2C_PORT_USB_SWITCH, i2c_addr, reg, val);
	if (res)
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

	if (pi3usb30532_reset(i2c_addr)) {
		CPRINTS("PI3USB30532 [%d] init failed", i2c_addr);
		return -1;
	}

	if (pi3usb30532_read(i2c_addr, PI3USB30532_REG_VENDOR, &val)) {
		CPRINTS("PI3USB30532 [%d] read failed", i2c_addr);
		return -1;
	} else if (val != PI3USB30532_VENDOR_ID) {
		CPRINTS("PI3USB30532 [%d] invalid ID 0x%02x", i2c_addr, val);
		return -1;
	}

	return 0;
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

const struct usb_mux_driver pi3usb30532_usb_mux_driver = {
	.init = pi3usb30532_init,
	.get = pi3usb30532_get_mux,
	.set = pi3usb30532_set_mux,
};
