/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PI3DPX1207 retimer.
 */

#include "pi3dpx1207.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "i2c.h"
#include "ioexpander.h"
#include "usb_mux.h"

static int pi3dpx1207_i2c_write(int i2c_port,
				uint16_t addr_flags,
				uint8_t offset,
				uint8_t val)
{
	int rv;
	uint8_t buff[32];

	/*
	 * PI3DPX1207 does not support device register offset in
	 * the typical I2C sense. Have to read the values starting
	 * from 0, modify the byte and then write the block.
	 */
	rv = i2c_xfer(i2c_port, addr_flags,
		      NULL, 0, buff, offset + 1);
	if (!rv) {
		buff[offset] = val;
		rv = i2c_xfer(i2c_port, addr_flags,
			      buff, offset + 1, NULL, 0);
	}
	return rv;
}

static int pi3dpx1207_init(int port, int i2c_port, uint16_t addr_flags)
{
	ioex_set_level(IOEX_USB_C0_DATA_EN, 1);
	return EC_SUCCESS;
}

static int pi3dpx1207_set_mux(int port,
			      int i2c_port, uint16_t addr_flags,
			      mux_state_t mux_state)
{
	int rv = EC_SUCCESS;
	uint8_t mode_val = PI3DPX1207_MODE_WATCHDOG_EN;

	if ((mux_state & MUX_USB_ENABLED) && !(mux_state & MUX_DP_ENABLED)) {
		gpio_set_level(GPIO_USB_C0_IN_HPD, 1);
		mode_val |= (mux_state & MUX_POLARITY_INVERTED)
				? PI3DPX1207_MODE_CONF_USB_FLIP
				: PI3DPX1207_MODE_CONF_USB;
	} else if (!(mux_state & MUX_USB_ENABLED)) {
		gpio_set_level(GPIO_USB_C0_IN_HPD, 0);
		mode_val |= PI3DPX1207_MODE_CONF_SAFE;
	} else {
		ccprintf("%s: mux state 0x%02X unimplemented\n",
			 __func__, mux_state);
		return EC_ERROR_UNIMPLEMENTED;
	}

	rv = pi3dpx1207_i2c_write(i2c_port, addr_flags,
				  PI3DPX1207_MODE_OFFSET,
				  mode_val);
	return rv;
}

const struct usb_retimer_driver pi3dpx1207_usb_retimer = {
	.init = pi3dpx1207_init,
	.set = pi3dpx1207_set_mux,
};
