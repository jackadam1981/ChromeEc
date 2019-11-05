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

	if ((mux_state & MUX_USB_ENABLED) && !(mux_state & MUX_DP_ENABLED)) {
		gpio_set_level(GPIO_USB_C0_IN_HPD, 1);
		if (mux_state & MUX_POLARITY_INVERTED) {
			rv = pi3dpx1207_i2c_write(i2c_port, addr_flags,
						  3, 0x52);
		} else {
			rv = pi3dpx1207_i2c_write(i2c_port, addr_flags,
						  3, 0x42);
		}
	} else if (!(mux_state & MUX_USB_ENABLED)) {
		gpio_set_level(GPIO_USB_C0_IN_HPD, 0);
		rv = pi3dpx1207_i2c_write(i2c_port, addr_flags,
					  3, 0x00);
	}
	return rv;
}

const struct usb_retimer_driver pi3dpx1207_usb_retimer = {
	.init = pi3dpx1207_init,
	.set = pi3dpx1207_set_mux,
};
