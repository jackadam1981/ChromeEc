/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "driver/tcpm/tcpci.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

/*
 * Default Port Discovery DR Swap Policy.
 *
 * 1) If port == 0 and dr_swap_to_dfp_flag == true and port data role is DFP,
 *    transition to pe_drs_send_swap
 * 2) If port == 1 and dr_swap_to_dfp_flag == true and port data role is UFP,
 *    transition to pe_drs_send_swap
 */
__overridable bool port_discovery_dr_swap_policy(int port,
			enum pd_data_role dr, bool dr_swap_flag)
{
	/*
	 * Port0: test if role is DFP
	 * Port1: test if role is UFP
	 */
	enum pd_data_role role_test = (port) ? PD_ROLE_UFP : PD_ROLE_DFP;

	if (dr_swap_flag && dr == role_test)
		return true;

	/* Do not perform a DR swap */
	return false;
}

/*
 * Default Port Discovery VCONN Swap Policy.
 *
 * 1) Never perform VCONN swap
 */
__overridable bool port_discovery_vconn_swap_policy(int port,
			bool vconn_swap_flag)
{
	/* Do not perform a VCONN swap */
	return false;
}


/*
 * TODO(b/167711550): These 4 functions need to be implemented for honeybuns
 * and are required to build with TCPMv2 enabled. Currently, they only allow the
 * build to work. They will be implemented in a subsequent CL.
 */

int pd_check_vconn_swap(int port)
{
	/*TODO: Dock is the Vconn source */
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
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	/*
	 * Default operation of buck-boost is 5v/3.6A.
	 * Turn on the PPC Provide Vbus.
	 */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

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

}
