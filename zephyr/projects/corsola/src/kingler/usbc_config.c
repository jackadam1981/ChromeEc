/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Kingler board-specific USB-C configuration */

#include "charger.h"
#include "console.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"

#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)

struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {};
struct ppc_config_t ppc_chips[CONFIG_USB_PD_PORT_MAX_COUNT] = {};
unsigned int ppc_cnt = ARRAY_SIZE(ppc_chips);
const struct charger_config_t chg_chips[] = {};
struct bc12_config bc12_ports[CONFIG_USB_PD_PORT_MAX_COUNT] = {};
struct usb_mux usb_muxes[CONFIG_USB_PD_PORT_MAX_COUNT] = {};

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
