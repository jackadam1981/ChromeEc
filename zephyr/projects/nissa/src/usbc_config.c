/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "driver/tcpm/tcpci.h"
#include "driver/tcpm/raa489000.h"

struct ppc_config_t ppc_chips[] = {};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_USB_C0_TCPC,
			.addr_flags = RAA489000_TCPC0_I2C_FLAGS,
		},
		.drv = &raa489000_tcpm_drv,
		/* RAA489000 implements TCPCI 2.0 */
		.flags = TCPC_FLAGS_TCPCI_REV2_0,
	},
	/* TODO(b:211693800) port 1 is present on sub-boards 1 and 2 with same
	 * configuration as port 0 but on I2C_PORT_USB_C1_TCPC.
	 */
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.usb_port = 0,
		.driver = &virtual_usb_mux_driver,
		.hpd_update = &virtual_hpd_update,
	},
	/* TODO(b:211693800) port 1 is present on sub-boards 1 and 2 with same
	 * configuration.
	 */
};

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on. */
	return chipset_in_state(CHIPSET_STATE_ANY_SUSPEND | CHIPSET_STATE_ON);
}

void pd_power_supply_reset(int port)
{
	/* Disable VBUS */
	tcpc_write(port, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SRC_CTRL_LOW);

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	if (port >= CONFIG_USB_PD_PORT_MAX_COUNT)
		return EC_ERROR_INVAL;

	/* Disable charging. */
	rv = tcpc_write(port, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SNK_CTRL_LOW);
	if (rv)
		return rv;

	/* Our policy is not to source VBUS when the AP is off. */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		return EC_ERROR_NOT_POWERED;

	/* Provide Vbus. */
	rv = tcpc_write(port, TCPC_REG_COMMAND, TCPC_REG_COMMAND_SRC_CTRL_HIGH);
	if (rv)
		return rv;

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}
