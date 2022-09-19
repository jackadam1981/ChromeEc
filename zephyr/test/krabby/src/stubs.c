/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_ramp.h"
#include "charge_state.h"
#include "usb_charge.h"

int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	return 0;
}

void board_set_charge_limit(int port, int supplier, int charge_ma, int max_ma,
			    int charge_mv)
{
}

int pd_check_vconn_swap(int port)
{
	return 0;
}

void pd_power_supply_reset(int port)
{
}

int pd_set_power_supply_ready(int port)
{
	return 1;
}

__attribute__((weak)) struct bc12_config bc12_ports[0];

__attribute__((weak)) void usb_charger_vbus_change(int port, int vbus_level)
{
}
