/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Parade PS8740 USB port switch driver.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "ps8740.h"
#include "usb_mux.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_USBCHARGE, outstr)
#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

static int ps8740_read(int i2c_addr, uint8_t reg, uint8_t *val)
{
	int read;

	if (i2c_read8(I2C_PORT_USB_MUX, i2c_addr, reg, &read))
		return -1;

	*val = read;
	return 0;
}

static int ps8740_write(int i2c_addr, uint8_t reg, uint8_t val)
{
	return i2c_write8(I2C_PORT_USB_MUX, i2c_addr, reg, val);
}

static int ps8740_reset(int i2c_addr)
{
	return ps8740_write(i2c_addr, PS8740_REG_MODE, PS8740_MODE_POWER_DOWN);
}

static int ps8740_init(int i2c_addr)
{
	uint8_t val;

	/* Reset chip back to power-on state */
	ps8740_reset(i2c_addr);

	/* Verify revision / chip ID registers */
	if (ps8740_read(i2c_addr, PS8740_REG_REVISION_ID1, &val) ||
	    val != PS8740_REVISION_ID1)
		return -1;

	if (ps8740_read(i2c_addr, PS8740_REG_REVISION_ID2, &val) ||
	    val != PS8740_REVISION_ID2)
		return -1;

	if (ps8740_read(i2c_addr, PS8740_REG_CHIP_ID1, &val) ||
	    val != PS8740_CHIP_ID1)
		return -1;

	if (ps8740_read(i2c_addr, PS8740_REG_CHIP_ID1, &val) ||
	    val != PS8740_CHIP_ID1)
		return -1;

	return 0;
}

/* Writes control register to set switch mode */
static int ps8740_set_mux(int i2c_addr, mux_state_t mux_state)
{
	uint8_t reg = 0;

	if (mux_state & MUX_USB_ENABLED)
		reg |= PS8740_MODE_USB_ENABLED;
	if (mux_state & MUX_DP_ENABLED)
		reg |= PS8740_MODE_DP_ENABLED;
	if (mux_state & MUX_POLARITY_INVERTED)
		reg |= PS8740_MODE_POLARITY_INVERTED;

	return ps8740_write(i2c_addr, PS8740_REG_MODE, reg);
}

/* Reads control register and updates mux_state accordingly */
static int ps8740_get_mux(int i2c_addr, mux_state_t *mux_state)
{
	uint8_t reg;

	*mux_state = 0;
	if (ps8740_read(i2c_addr, PS8740_REG_STATUS, &reg))
		return -1;

	if (reg & PS8740_STATUS_USB_ENABLED)
		*mux_state |= MUX_USB_ENABLED;
	if (reg & PS8740_STATUS_DP_ENABLED)
		*mux_state |= MUX_DP_ENABLED;
	if (reg & PS8740_STATUS_POLARITY_INVERTED)
		*mux_state |= MUX_POLARITY_INVERTED;

	return 0;
}

const struct usb_mux_driver ps8740_usb_mux_driver = {
	.init = ps8740_init,
	.set = ps8740_set_mux,
	.get = ps8740_get_mux,
};
