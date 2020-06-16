/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TI TUSB1064
 * USB Type-C Redriving Switch for USB Host / DisplayPort.
 */

#include "common.h"
#include "console.h"
#include "i2c.h"
#include "tusb1064.h"
#include "usb_mux.h"
#include "util.h"

int tusb1064_read(const struct usb_mux *me, uint8_t reg, int *val)
{
	return i2c_read8(me->i2c_port, me->i2c_addr_flags,
			 reg, val);
}

int tusb1064_write(const struct usb_mux *me, uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags,
			  reg, val);
}

static int tusb1064_init(const struct usb_mux *me)
{
	int rv;
	int reg;

	rv = tusb1064_read(me, TUSB1064_REG_MODE, &reg);
	if (rv) {
		ccprintf("tusb1064: mux init failed");
		return rv;
	}

	return EC_SUCCESS;
}

/* Writes control register to set switch mode */
static int tusb1064_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	int reg;
	int rv;

	rv = tusb1064_read(me, TUSB1064_REG_MODE, &reg);
	if (rv)
		return rv;

	/* Assume standby */
	reg &= ~(TUSB1064_MODE_ALT_DP_EN | TUSB1064_MODE_USB_EN |
		 TUSB1064_MODE_FLIPSEL);

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg = TUSB1064_MODE_USB_EN;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= TUSB1064_MODE_ALT_DP_EN;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= TUSB1064_MODE_FLIPSEL;

	ccprintf("tusb1064: set_mux: = 0x%x\n", mux_state);
	tusb1064_write(me, TUSB1064_REG_MODE, reg);

	return EC_SUCCESS;
}

/* Reads control register and updates mux_state accordingly */
static int tusb1064_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;

	tusb1064_read(me, TUSB1064_REG_MODE, &reg);

	*mux_state = 0;
	if (reg & TUSB1064_MODE_USB_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & TUSB1064_MODE_ALT_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & TUSB1064_MODE_FLIPSEL)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	ccprintf("tusb1064: get_mux: = 0x%x\n", *mux_state);

	return EC_SUCCESS;
}


const struct usb_mux_driver tusb1064_usb_mux_driver = {
	.init = tusb1064_init,
	.set = tusb1064_set_mux,
	.get = tusb1064_get_mux,
};
