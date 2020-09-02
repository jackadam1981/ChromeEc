/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Shared USB-C policy for Volteer boards */
#include "charge_manager.h"
#include "chipset.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "usb_pd.h"
#include "system.h"

#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#define UFP_FLAG_ALT_MODE_TBT	BIT(0)

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

int pd_snk_is_vbus_provided(int port)
{
	return ppc_is_vbus_present(port);
}

int board_vbus_source_enabled(int port)
{
	return ppc_is_sourcing_vbus(port);
}

/* ----------------- Vendor Defined Messages ------------------ */
#define OPOS_TBT 1

union tbt_dev_mode_enter_cmd ufp_enter_mode[CONFIG_USB_PD_PORT_MAX_COUNT];
static uint8_t ufp_alt_mode_flags[CONFIG_USB_PD_PORT_MAX_COUNT];

const uint32_t vdo_tbt_modes[1] = {
	VDO_MODE_TBT_DEV(0,      /* no vendor specific B1 supported */
			 0,      /* no vendor specific B0 supported */
			 1,      /* Intel specific B0 supported */
			 0,      /* TBT3 adapter */
			 0x0001) /* TBT mode */
};

const uint32_t vdo_idh = VDO_IDH(1, /* Data caps as USB host   */
				 1, /* Data caps as USB device */
				 IDH_PTYPE_HUB, /* 1: Hub */
				 1, /* Supports alt modes */
				 USB_VID_GOOGLE) |
				 (((0x2) & 0x7) << 23); /* PDUSB host */

const uint32_t vdo_product = VDO_PRODUCT(CONFIG_USB_PID, 0);

/* TODO: add USB4 to capability once USB4 response implemented */
const uint32_t vdo_ufp1 = VDO_UFP1(0x5, /* USB2.0, USB3.2 device capable */
				   0x3, /* TBT3, reconfig */
				   0x3  /* USB4 Gen3 */);


const uint32_t vdo_dfp = VDO_DFP(7, /* USB2.0, USB3.2 and USB4 host capable */
				 1  /* Port 1 */);

__overridable union tbt_dev_mode_enter_cmd pd_ufp_get_enter_mode(int port)
{
	return ufp_enter_mode[port];
}

__overridable void ufp_clear_alt_mode(int port)
{
	ufp_alt_mode_flags[port] = 0;
}

__overridable void ufp_mux_set_alt_mode(int port)
{
	if (ufp_alt_mode_flags[port] & UFP_FLAG_ALT_MODE_TBT)
		set_tbt_compat_mode_ready(port);
}

static bool is_port_ready_to_respond(int port)
{
	/*
	 * To enter any alternate mode successfully, the Burnside Bridge needs
	 * to be set correctly. So, respond BUSY till the main AP rail isn't
	 * powered as, Burnside Bridge is powered by main AP rail.
	 */
	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		return false;

	return true;
}

static int svdm_tbt_compat_response_identity(int port, uint32_t *payload)
{
	if (!is_port_ready_to_respond(port))
		return -1;

	payload[VDO_I(IDH)] = vdo_idh;
	payload[VDO_I(CSTAT)] = VDO_CSTAT(0);
	payload[VDO_I(PRODUCT)] = vdo_product;
	payload[VDO_I(PTYPE_UFP1_VDO)] = vdo_ufp1;
	payload[VDO_I(PTYPE_UFP2_VDO)] = 0;
	payload[VDO_I(PTYPE_DFP_VDO)] = vdo_dfp;
	return VDO_I(PTYPE_DFP_VDO) + 1;
}

static int svdm_tbt_compat_response_svids(int port, uint32_t *payload)
{
	if (!is_port_ready_to_respond(port))
		return -1;

	payload[1] = VDO_SVID(USB_VID_INTEL, 0);
	return 2;
}

static int svdm_tbt_compat_response_modes(int port, uint32_t *payload)
{
	if (!is_port_ready_to_respond(port))
		return -1;

	if (PD_VDO_VID(payload[0]) == USB_VID_INTEL) {
		memcpy(payload + 1, vdo_tbt_modes, sizeof(vdo_tbt_modes));
		return ARRAY_SIZE(vdo_tbt_modes) + 1;
	} else {
		return 0; /* NAK */
	}
}

static int svdm_tbt_compat_response_enter_mode(
	int port, uint32_t *payload)
{
	mux_state_t mux_state = 0;

	/* Do not enter mode while CPU is off. */
	if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF))
		return 0; /* NAK */

	if ((PD_VDO_VID(payload[0]) == USB_VID_INTEL) &&
		(PD_VDO_OPOS(payload[0]) == OPOS_TBT)) {
		mux_state = usb_mux_get(port);

		/*
		 * Ref: Figure 6-21 Successful Enter Mode sequence
		 * UFP(responder) should be in USB mode or safe mode before
		 * sending Enter Mode Command response.
		 */
		if ((mux_state & USB_PD_MUX_USB_ENABLED) ||
			(mux_state & USB_PD_MUX_SAFE_MODE)) {

			/* Save TBT3 SOP VDO from enter_mode request */
			ufp_enter_mode[port] =
				(union tbt_dev_mode_enter_cmd)payload[1];

			/*
			 * TODO(b/166455363): Object position in Enter Mode
			 * Command response VDM header is the offset of mode
			 * entered, make sure it's set correctly in response
			 * message buffer.
			 */
			set_tbt_compat_mode_ready(port);
			ufp_alt_mode_flags[port] |= UFP_FLAG_ALT_MODE_TBT;

			CPRINTS("UFP Enter TBT mode");
			return 1; /* ACK */
		}
	}

	ufp_alt_mode_flags[port] &= ~UFP_FLAG_ALT_MODE_TBT;
	CPRINTS("UFP failed to enter TBT mode(mux=0x%x)", mux_state);
	return 0;
}

static int svdm_exit_mode(int port, uint32_t *payload)
{
	if ((PD_VDO_VID(payload[0]) == USB_VID_INTEL) &&
		(PD_VDO_OPOS(payload[0]) == OPOS_TBT)) {
		ufp_enter_mode[port].raw_value = 0;
		ufp_alt_mode_flags[port] &= ~UFP_FLAG_ALT_MODE_TBT;
		usb_mux_set(port, USB_PD_MUX_USB_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
		return 1;
	}
	return 0;
}

const struct svdm_response svdm_rsp = {
	.identity = &svdm_tbt_compat_response_identity,
	.svids = &svdm_tbt_compat_response_svids,
	.modes = &svdm_tbt_compat_response_modes,
	.enter_mode = &svdm_tbt_compat_response_enter_mode,
	.amode = NULL,
	.exit_mode = &svdm_exit_mode,
};
