/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_mux.h"
#include "usbc_ppc.h"
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
