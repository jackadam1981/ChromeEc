/*
 * Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Parade PS8828 USB/DP Mux.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/retimer/ps8828.h"
#include "hooks.h"
#include "i2c.h"
#include "queue.h"
#include "timer.h"
#include "usb_mux.h"
#include "usbc/usb_muxes.h"

#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ##args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ##args)

static int ps8828_read(const struct usb_mux *me, int page, uint8_t reg,
		       int *val)
{
	CPRINTSUSB("%s: %02x", __func__, me->i2c_addr_flags + page + 1);
	return i2c_read8(me->i2c_port, me->i2c_addr_flags + page + 1, reg, val);
}

static int ps8828_write(const struct usb_mux *me, int page, uint8_t reg,
			int val)
{
	CPRINTSUSB("%s: %02x", __func__, me->i2c_addr_flags + page);
	return i2c_write8(me->i2c_port, me->i2c_addr_flags + page, reg, val);
}

/* Writes control register to set switch mode */
static int ps8828_set_mux(const struct usb_mux *me, mux_state_t mux_state,
			  bool *ack_required)
{
	int reg;
	int rv;

	CPRINTSUSB("RPZ mux set");
	/* This driver does not use host command ACKs */
	*ack_required = false;

	/* This driver treats safe mode as none */
	if (mux_state & USB_PD_MUX_SAFE_MODE)
		mux_state = USB_PD_MUX_NONE;

	rv = ps8828_read(me, PS8828_REG_PAGE0, PS8828_REG_MODE, &reg);
	if (rv) {
		CPRINTSUSB("PS8828 read failed %d", rv);
		return rv;
	}

	reg &= ~(PS8828_MODE_ALT_DP_EN | PS8828_MODE_USB_EN | PS8828_MODE_FLIP);

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= PS8828_MODE_USB_EN;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= PS8828_MODE_ALT_DP_EN;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= PS8828_MODE_FLIP;

	rv = ps8828_write(me, PS8828_REG_PAGE0, PS8828_REG_MODE, reg);
	if (rv)
		CPRINTSUSB("PS8828 write failed %d", rv);

	return rv;
}

/* Reads control register and updates mux_state accordingly */
static int ps8828_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;
	int rv;

	rv = ps8828_read(me, PS8828_REG_PAGE0, PS8828_REG_MODE, &reg);
	if (rv)
		return rv;

	*mux_state = 0;
	if (reg & PS8828_MODE_USB_EN)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & PS8828_MODE_ALT_DP_EN)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & PS8828_MODE_FLIP)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}

const struct usb_mux_driver ps8828_usb_retimer_driver = {
	.set = ps8828_set_mux,
	.get = ps8828_get_mux,
};
