/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PS8802 retimer.
 */

#include "ps8802.h"
#include "common.h"
#include "console.h"
#include "i2c.h"
#include "usb_mux.h"

/*
 * If PS8802 is in I2C standby mode, wake it up by reading PS8802_REG_MODE.
 * The first read will fail, the second should succeed.
 */
static int ps8802_i2c_wake(int port)
{
	uint8_t tmp;
	int rv = EC_ERROR_UNKNOWN;

	i2c_lock(port, 1);
	for (int i = 0; (i < 2) && (rv != EC_SUCCESS); i++) {
		/*
		 * Talk to all three addresses. This seems to get the
		 * chip out of its sleeping mode where just reading
		 * one I2C address at the PS8802_REG_MODE does not.
		 */
		rv = i2c_xfer_unlocked(usb_retimers[port].i2c_port,
				       usb_retimers[port].i2c_addr_flags,
				       NULL, 0, &tmp, 1, I2C_XFER_SINGLE);
		rv |= i2c_xfer_unlocked(usb_retimers[port].i2c_port,
				       usb_retimers[port].i2c_addr_flags + 1,
				       NULL, 0, &tmp, 1, I2C_XFER_SINGLE);
		rv |= i2c_xfer_unlocked(usb_retimers[port].i2c_port,
				       usb_retimers[port].i2c_addr_flags + 2,
				       NULL, 0, &tmp, 1, I2C_XFER_SINGLE);
	}
	i2c_lock(port, 0);

	return rv;
}

static int ps8802_i2c_write(int port, int offset, int data)
{
	return i2c_write8(usb_retimers[port].i2c_port,
			  usb_retimers[port].i2c_addr_flags,
			  offset, data);
}

static int ps8802_set_mux(int port, mux_state_t mux_state)
{
	int val = (PS8802_MODE_DP_REG_CONTROL
		   | PS8802_MODE_USB_REG_CONTROL
		   | PS8802_MODE_FLIP_REG_CONTROL
		   | PS8802_MODE_IN_HPD_REG_CONTROL);
	int rv;

	rv = ps8802_i2c_wake(port);
	if (rv)
		return rv;

	if (mux_state & MUX_USB_ENABLED)
		val |= PS8802_MODE_USB_ENABLE;
	if (mux_state & MUX_DP_ENABLED)
		val |= PS8802_MODE_DP_ENABLE;
	if (mux_state & MUX_POLARITY_INVERTED)
		val |= PS8802_MODE_FLIP_ENABLE;

	return ps8802_i2c_write(port, PS8802_REG_MODE, val);
}

const struct usb_retimer_driver ps8802_usb_retimer = {
	.set = ps8802_set_mux,
};
