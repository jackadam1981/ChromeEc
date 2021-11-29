/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kingler board-specific USB-C configuration */

#include "charger.h"
#include "console.h"
#include "driver/bc12/pi3usb9201_public.h"
#include "driver/charger/isl923x_public.h"
#include "driver/tcpm/rt1718s.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#include "baseboard_usbc_config.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {};
struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);

const struct charger_config_t chg_chips[] = {
	{
		.i2c_port = I2C_PORT_CHARGER,
		.i2c_addr_flags = ISL923X_ADDR_FLAGS,
		.drv = &isl923x_drv,
	}
};

struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {};

struct bc12_config bc12_ports[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USBC_PORT_C0] = {
		.drv = &pi3usb9201_drv,
	},
	[USBC_PORT_C0] = {
		.drv = &rt1718s_bc12_drv,
	}
};

const struct pi3usb9201_config_t
		pi3usb9201_bc12_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	[USBC_PORT_C0] = {
		.i2c_port = I2C_PORT_USB_C0,
		.i2c_addr_flags = PI3USB9201_I2C_ADDR_3_FLAGS,
	},
	[USBC_PORT_C1] = { /* unused */ }
};

void board_reset_pd_mcu(void)
{
}

int board_set_active_charge_port(int charge_port)
{
	return EC_SUCCESS;
}

void pd_power_supply_reset(int port)
{
}

uint16_t tcpc_get_alert_status(void)
{
	return 0;
}

int pd_set_power_supply_ready(int port)
{
	return EC_SUCCESS;
}

int pd_check_vconn_swap(int port)
{
	return EC_SUCCESS;
}
