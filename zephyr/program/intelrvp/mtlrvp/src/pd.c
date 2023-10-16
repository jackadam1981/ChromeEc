/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"

#if 0
void board_dc_jack_interrupt(enum gpio_signal signal)
{
}
#endif

__override uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}

void board_charging_enable(int port, int enable)
{
}

void pd_set_new_power_request(int port)
{
}

void pd_request_power_swap(int port)
{
}

int board_vbus_source_enabled(int port)
{
	return 0;
}
