<<<<<<< HEAD
/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "tusb1064.h"
#include "usb_mux.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBCHARGE, format, ## args)

/*
 * configuration bits which never change in the General Register
 * e.g. REG_GENERAL_DP_EN_CTRL or REG_GENERAL_EQ_OVERRIDE
 */
#define REG_GENERAL_STATIC_BITS REG_GENERAL_EQ_OVERRIDE

static int tusb1064_read(const struct usb_mux *me, uint8_t reg, uint8_t *val)
{
	int buffer = 0xee;
	int res = i2c_read8(me->i2c_port, me->i2c_addr_flags,
			    (int)reg, &buffer);
	*val = buffer;
	return res;
}

static int tusb1064_write(const struct usb_mux *me, uint8_t reg, uint8_t val)
{
	return i2c_write8(me->i2c_port, me->i2c_addr_flags,
			  (int)reg, (int)val);
=======
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
>>>>>>> 8b8f730b0f (tusb1064: Create usb mux driver for tsub1064)
}

/* Writes control register to set switch mode */
static int tusb1064_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
<<<<<<< HEAD
	int reg = REG_GENERAL_STATIC_BITS;

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= REG_GENERAL_CTLSEL_USB3;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= REG_GENERAL_CTLSEL_ANYDP;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= REG_GENERAL_FLIPSEL;

	return tusb1064_write(me, TUSB1064_REG_GENERAL, reg);
=======
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
>>>>>>> 8b8f730b0f (tusb1064: Create usb mux driver for tsub1064)
}

/* Reads control register and updates mux_state accordingly */
static int tusb1064_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
<<<<<<< HEAD
	uint8_t reg;
	int res;

	res = tusb1064_read(me, TUSB1064_REG_GENERAL, &reg);
	if (res)
		return EC_ERROR_INVAL;

	*mux_state = 0;
	if (reg & REG_GENERAL_CTLSEL_USB3)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & REG_GENERAL_CTLSEL_ANYDP)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & REG_GENERAL_FLIPSEL)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

/* Generic driver init function */
static int tusb1064_init(const struct usb_mux *me)
{
	int res;
	uint8_t reg;

	/* Default to "Floating Pin" DP Equalization */
	reg = TUSB1064_DP1EQ(TUSB1064_DP_EQ_RX_10_0_DB) |
		TUSB1064_DP3EQ(TUSB1064_DP_EQ_RX_10_0_DB);
	res = tusb1064_write(me, TUSB1064_REG_DP1DP3EQ_SEL, reg);
	if (res)
		return res;

	reg = TUSB1064_DP0EQ(TUSB1064_DP_EQ_RX_10_0_DB) |
		TUSB1064_DP2EQ(TUSB1064_DP_EQ_RX_10_0_DB);
	res = tusb1064_write(me, TUSB1064_REG_DP0DP2EQ_SEL, reg);
	if (res)
		return res;

	/* Disconnect USB3.1 and DP */
	res = tusb1064_set_mux(me, USB_PD_MUX_NONE);
	if (res)
		return res;

	/* Disable AUX mux override */
	res = tusb1064_write(me, TUSB1064_REG_AUXDPCTRL, 0);
	if (res)
		return res;
=======
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
>>>>>>> 8b8f730b0f (tusb1064: Create usb mux driver for tsub1064)

	return EC_SUCCESS;
}

<<<<<<< HEAD
const struct usb_mux_driver tusb1064_usb_mux_driver = {
	/* CAUTION: This is an UFP/RX/SINK redriver mux */
=======

const struct usb_mux_driver tusb1064_usb_mux_driver = {
>>>>>>> 8b8f730b0f (tusb1064: Create usb mux driver for tsub1064)
	.init = tusb1064_init,
	.set = tusb1064_set_mux,
	.get = tusb1064_get_mux,
};
