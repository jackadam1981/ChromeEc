/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "i2c.h"
#include "tusb1064.h"
#include "ioexpanders.h"

int init_tusb1064(int port)
{
	uint8_t val;


	switch(board_id_cached()){

	case BOARD_ID_PROTO1:
		//PROTO1
		/* Enable the TUSB1064 redriver */
		// Actually a NOOP since I2C_EN is PP3300 on PROTO1
		cmux_en(1);
	break;
	case BOARD_ID_EVT1:
		//EVT1

		

	default:
	}

	/* Disconnect USB3.1 and DP */
	val = tusb1064_read_byte(port, TUSB1064_REG_GENERAL);
	if (val < 0)
		return EC_ERROR_INVAL;

	/* Read Modify Write */
	val &= ~REG_GENERAL_CTLSEL_MASK;
	val |= REG_GENERAL_CTLSEL_DISABLE;
	if (tusb1064_write_byte(port, TUSB1064_REG_GENERAL, val))
		return EC_ERROR_INVAL;

	return EC_SUCCESS;
}

int tusb1064_write_byte(int port, uint8_t reg, uint8_t val)
{
	return i2c_write8(port, TUSB1064_ADDR_FLAGS, reg, val);
}

int tusb1064_read_byte(int port, uint8_t reg)
{
	int tmp;

	if (i2c_read8(port, TUSB1064_ADDR_FLAGS, reg, &tmp))
		return -1;

	return tmp;
}


/* Writes control register to set switch mode */
static int tusb1064_set_mux(const struct usb_mux *me, mux_state_t mux_state)
{
	/*
	 * For CE_DP, CE_USB, and FLIP, disable pin control and enable I2C
	 * control.
	 */
	uint8_t reg = (TUSB1064_MODE_IN_HPD_CONTROL | // < - doesn't exist
		       TUSB1064_MODE_DP_REG_CONTROL |
		       TUSB1064_MODE_USB_REG_CONTROL |
		       TUSB1064_MODE_FLIP_REG_CONTROL);

	if (mux_state & USB_PD_MUX_USB_ENABLED)
		reg |= TUSB1064_MODE_USB_ENABLE;
	if (mux_state & USB_PD_MUX_DP_ENABLED)
		reg |= TUSB1064_MODE_DP_ENABLE | TUSB1064_MODE_IN_HPD_ASSERT;
	if (mux_state & USB_PD_MUX_POLARITY_INVERTED)
		reg |= TUSB1064_MODE_FLIP_ENABLE;

	return tusb1064_write(me, TUSB1064_REG_GENERAL, reg);
}

/* Reads control register and updates mux_state accordingly */
static int tusb1064_get_mux(const struct usb_mux *me, mux_state_t *mux_state)
{
	int reg;
	int res;

	res = tusb1064_read(me, TUSB1064_REG_GENERAL, &reg);
	if (res)
		return res;

	*mux_state = 0;
	if (reg & TUSB1064_STATUS_USB_ENABLED)
		*mux_state |= USB_PD_MUX_USB_ENABLED;
	if (reg & TUSB1064_STATUS_DP_ENABLED)
		*mux_state |= USB_PD_MUX_DP_ENABLED;
	if (reg & TUSB1064_STATUS_POLARITY_INVERTED)
		*mux_state |= USB_PD_MUX_POLARITY_INVERTED;

	return EC_SUCCESS;
}


const struct usb_mux_driver tusb1064_usb_mux_driver = {
	.init = tusb1064_init,
	.set = tusb1064_set_mux,
	.get = tusb1064_get_mux,
};