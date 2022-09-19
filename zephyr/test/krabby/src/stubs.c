/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "usbc_ppc.h"
#include "charge_ramp.h"
#include "charge_state.h"

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

void pd_set_vbus_discharge(int port, int enable)
{
}

int board_get_battery_soc(void)
{
	return 0;
}
