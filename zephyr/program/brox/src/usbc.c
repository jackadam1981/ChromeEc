/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Brox USB-C board functions
 */

#include "ec_commands.h"
#include "host_command.h"
#include "usb_pd.h"
#include "usbc/pdc_power_mgmt.h"

enum ec_pd_port_location board_get_pd_port_location(int port)
{
	switch (port) {
	case 0:
		return EC_PD_PORT_LOCATION_LEFT_BACK;
	case 1:
		return EC_PD_PORT_LOCATION_LEFT_FRONT;
	}
	return EC_PD_PORT_LOCATION_UNKNOWN;
}

__override int board_get_vbus_voltage(int port)
{
	return pdc_power_mgmt_get_vbus_voltage(port);
	;
}
