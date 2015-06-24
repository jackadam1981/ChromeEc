/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB mux high-level driver. */


void usb_mux_init(int port)
{
	ASSERT(port < ARRAY_SIZE(usb_muxes));
	usb_muxes[port].driver->init(usb_muxes[port].i2c_port);
}
