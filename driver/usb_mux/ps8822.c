/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Parade PS8740 (and PS8742)
 * USB Type-C Redriving Switch for USB Host / DisplayPort.
 */

#include "common.h"
#include "i2c.h"
#include "ps8822.h"
#include "usb_mux.h"
#include "util.h"

int ps8822_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags,
			 reg, val);
}

int ps8822_write(const struct usb_mux *me, uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags,
			  reg, val);
}

static int ps8822_init(const struct usb_mux *me)
{
	int id1;
	int res;

	/* Reset chip back to power-on state */
	res = ps8822_read(me, PS8822_REG_MODE, &id1);
	if (res)
		return res;

	/*
	 * Verify chip ID registers.
	 */

	return EC_SUCCESS;
}

/* Writes control register to set switch mode */
static int ps8822_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	int reg;
	int rv;

	rv = ps8822_read(me, PS8822_REG_MODE, &reg);
	if (rv)
		return rv;

	/* Assume standby, preserve PIN_E config bit */
	reg &= ~(PS8822_MODE_ALT_DP_EN | PS8822_MODE_USB_EN | PS8822_MODE_FLIP);

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg = PS8822_MODE_USB_EN;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= PS8822_MODE_ALT_DP_EN;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= PS8822_MODE_FLIP;

	return ps8822_write(me, PS8822_REG_MODE, reg);
}

/* Reads control register and updates mux_state accordingly */
static int ps8822_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;
	int res;

	res = ps8822_read(me, PS8822_REG_MODE, &reg);
	if (res)
		return res;

	*mux_state = 0;
	if (reg & PS8822_MODE_USB_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & PS8822_MODE_ALT_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & PS8822_MODE_FLIP)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}


const struct usb_mux_driver ps8822_usb_mux_driver = {
	.init = ps8822_init,
	.set = ps8822_set_mux,
	.get = ps8822_get_mux,
};
