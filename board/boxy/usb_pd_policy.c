/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charger.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "usb_pd.h"
#include "usbc_ppc.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)

int pd_check_vconn_swap(int port)
{
	/* Allow VCONN swaps if the AP is on */
	return gpio_get_level(GPIO_EN_PP5000_U);
}

static void notify_power_change(void)
{
	/* Notify host of power info change. */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}
DECLARE_DEFERRED(notify_power_change);

void pd_power_supply_reset(int port)
{
	int prev_en;

	if (port < 0 || port >= board_get_usb_pd_port_count())
		return;

	prev_en = ppc_is_sourcing_vbus(port);

	/* Disable VBUS source */
	ppc_vbus_source_enable(port, 0);

	/* Enable discharge if we were previously sourcing 5V */
	if (prev_en)
		pd_set_vbus_discharge(port, 1);

	/* Notify host of power info change. */
	hook_call_deferred(&notify_power_change_data, 0);
}

int pd_set_power_supply_ready(int port)
{
	int rv;

	/* Disable charging */
	rv = ppc_vbus_sink_enable(port, 0);
	if (rv)
		return rv;

	pd_set_vbus_discharge(port, 0);

	/* Enable VBUS source */
	rv = ppc_vbus_source_enable(port, 1);
	if (rv)
		return rv;

	/* Notify host of power info change. */
	hook_call_deferred(&notify_power_change_data, 0);

	return EC_SUCCESS;
}

__override int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

static int svdm_identity(int port, uint32_t *payload)
{
	/* The SVID in the Discover Identity Command request Shall be set to the
	 * PD SID */
	if (PD_VDO_VID(payload[VDO_INDEX_HDR]) != USB_SID_PD) {
		return 0;
	}

	payload[VDO_I(CSTAT)] = VDO_CSTAT(CONFIG_USB_PD_XID);
	payload[VDO_I(PRODUCT)] =
		VDO_PRODUCT(CONFIG_USB_PID, CONFIG_USB_BCD_DEV);

	if (pd_get_rev(port, TCPCI_MSG_SOP) < PD_REV30) {
		payload[VDO_I(IDH)] = VDO_IDH(1, /* USB host */
					      0, /* Not a USB device */
					      IDH_PTYPE_UNDEF, /* Not a UFP */
					      0, /* No alt modes (not a UFP) */
					      CONFIG_USB_VID);

		return VDO_I(PRODUCT) + 1;
	} else {
		payload[VDO_I(IDH)] =
			VDO_IDH_REV30(1, /* USB host */
				      0, /* Not a USB device */
				      IDH_PTYPE_UNDEF, /* Not a UFP */
				      0, /* No alt modes (not a UFP) */
				      IDH_PTYPE_DFP_HOST, /* PDUSB host */
				      USB_TYPEC_RECEPTACLE, CONFIG_USB_VID);

		/* Single VDO for DFP product type */
		payload[VDO_I(PRODUCT) + 1] =
			VDO_DFP(VDO_DFP_HOST_CAPABILITY_USB32,
				USB_TYPEC_RECEPTACLE, port);

		return VDO_I(PRODUCT) + 2;
	}
}

/* 6.4.4.3.2 A Responder that does not support any SVIDs Shall return a NAK.*/
static int svdm_svids(int port, uint32_t *payload)
{
	return 0;
}

__override const struct svdm_response svdm_rsp = {
	.identity = svdm_identity,
	.svids = svdm_svids,
	/*
	 * Discover Identity support is required for devices with more than one
	 * DFP, but other SVDM commands are optional. We don't support operating
	 * as Responder in any mode, so leave them unimplemented. See 6.13.5,
	 * Applicability of Structured VDM Commands.
	 */
};
