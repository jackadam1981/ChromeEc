/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "usbc/pdc_power_mgnt.h"
#include "usb_mux.h"

uint8_t board_get_usb_pd_port_count(void)
{
	return CONFIG_USB_PD_PORT_MAX_COUNT;
}
void board_charging_enable(int port, int en)
{
}

void pd_request_vconn_swap(int port)
{
}

void pd_set_dual_role(int port, enum pd_dual_role_states state)
{
}

void pd_request_power_swap(int port)
{
}

void pd_set_new_power_request(int port)
{
}

int board_get_vbus_voltage(int port)
{
	return tcpm_get_vbus_voltage(port);
}

int board_vbus_source_enabled(int port)
{
	return 0;
}
