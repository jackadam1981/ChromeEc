/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "chip/stm32/ucpd-stm32gx.h"
#include "driver/tcpm/tcpci.h"
#include "mp4245.h"
#include "timer.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int pd_check_vconn_swap(int port)
{
	/*TODO: Dock is the Vconn source */
//	return EC_SUCCESS;
	return 1;
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	if (port < 0 || port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	/* Turn off voltage output from buck-boost */
	mp4245_votlage_out_enable(0);

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	//ucpd_info(port);

	CPRINTS("pd: power supply ready");

	/* Ensure buck-boost is enabled and Vout is on */
	mp4245_set_voltage_out(5000);
	mp4245_votlage_out_enable(1);
	msleep(2);
	//ucpd_info(port);
	/*
	 * Default operation of buck-boost is 5v/3.6A.
	 * Turn on the PPC Provide Vbus.
	 */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	CPRINTS("pd: vbus source enabled");

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;

}

int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

void pd_set_input_current_limit(int port, uint32_t max_ma,
				uint32_t supply_voltage)
{
	/* No battery, nothing to do */
	return;
}
