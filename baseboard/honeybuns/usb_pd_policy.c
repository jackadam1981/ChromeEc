/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery_smart.h"
#include "charge_manager.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "driver/charger/sm5803.h"
#include "driver/tcpm/tcpci.h"
#include "usb_pd.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on */
	return chipset_in_state(CHIPSET_STATE_ANY_SUSPEND | CHIPSET_STATE_ON);
}

void pd_power_supply_reset(int port)
{

	if (port < 0 || port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return;

	/* Disable Vbus */


	/* Discharge Vbus if previously enabled */


}

int pd_set_power_supply_ready(int port)
{

	return EC_SUCCESS;
}

int pd_snk_is_vbus_provided(int port)
{
	int chg_det = 0;


	return chg_det;
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	/* No battery, nothing to do */
	return;
}
