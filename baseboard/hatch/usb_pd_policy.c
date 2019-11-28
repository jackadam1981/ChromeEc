/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Shared USB-C policy for Hatch boards */

#include "charge_manager.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "system.h"
#include "tcpci.h"
#include "tcpm.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

int pd_check_vconn_swap(int port)
{
	/* Only allow vconn swap if pp5000_A rail is enabled */
	return gpio_get_level(GPIO_EN_PP5000_A);
}

void pd_power_supply_reset(int port)
{
	int prev_en;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS. */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	/* Give back the current quota we are no longer using */
	charge_manager_source_port(port, 0);
#endif /* defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	/* Disable charging. */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv)
		return rv;

	pd_set_vbus_discharge(port, 0);

	/* Provide Vbus. */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	/* Ensure we advertise the proper available current quota */
	charge_manager_source_port(port, 1);
#endif /* defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT) */

	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_VBUS_DETECT_PPC
int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}
#endif

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}


/* ----------------- Vendor Defined Messages ------------------ */
__override void svdm_dp_post_config(int port)
{
	const struct usb_mux *mux = &usb_muxes[port];

	dp_flags[port] |= DP_FLAGS_DP_ON;
	if (!(dp_flags[port] & DP_FLAGS_HPD_HI_PENDING))
		return;
	mux->hpd_update(port, 1, 0);
#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, 1);
#endif
}

__override int svdm_dp_attention(int port, uint32_t *payload)
{
	int lvl = PD_VDO_DPSTS_HPD_LVL(payload[1]);
	int irq = PD_VDO_DPSTS_HPD_IRQ(payload[1]);
	const struct usb_mux *mux = &usb_muxes[port];

	dp_status[port] = payload[1];
	if (!(dp_flags[port] & DP_FLAGS_DP_ON)) {
		if (lvl)
			dp_flags[port] |= DP_FLAGS_HPD_HI_PENDING;
		return 1;
	}
	mux->hpd_update(port, lvl, irq);
#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, lvl);
#endif

	/* ack */
	return 1;
}
__override void svdm_exit_dp_mode(int port)
{
	const struct usb_mux *mux = &usb_muxes[port];

	svdm_safe_dp_mode(port);
	mux->hpd_update(port, 0, 0);
#ifdef USB_PD_PORT_TCPC_MST
	if (port == USB_PD_PORT_TCPC_MST)
		baseboard_mst_enable_control(port, 0);
#endif
}
