/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Virtual USB mux driver. Track mux state for host control.
 */

#include "common.h"
#include "console.h"
#include "host_command.h"
#include "usb_mux.h"
#include "util.h"

static mux_state_t virtual_mux_state[CONFIG_USB_PD_PORT_COUNT];

static int virtual_init(int port)
{
	return EC_SUCCESS;
}

static int virtual_set_mux(int port, mux_state_t mux_state)
{
	if (virtual_mux_state[port] != mux_state) {
		virtual_mux_state[port] = mux_state;
		host_set_single_event(EC_HOST_EVENT_USB_MUX);
	}
	return EC_SUCCESS;
}

static int virtual_get_mux(int port, mux_state_t *mux_state)
{
	*mux_state = virtual_mux_state[port];
	return EC_SUCCESS;
}

const struct usb_mux_driver virtual_usb_mux_driver = {
	.init = virtual_init,
	.set = virtual_set_mux,
	.get = virtual_get_mux,
};
