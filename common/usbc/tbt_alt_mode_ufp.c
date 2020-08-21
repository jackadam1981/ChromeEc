/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/*
 * Thunderbolt UFP alternate mode support
 * Refer to USB Power Delivery Specification Revision 3.0,
 * Version 2.0 Section 6.4.4 Vendor Defined Message
 */

#include "charge_manager.h"
#include "compile_time_macros.h"
#include "console.h"
#include "gpio.h"
#include "usb_common.h"
#include "usb_mux.h"
#include "usbc_ppc.h"
#include "usb_pd.h"
#include "system.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

#define OPOS_TBT 1

union tbt_dev_mode_enter_cmd dfp_enter_mode[CONFIG_USB_PD_PORT_MAX_COUNT];

__override union tbt_dev_mode_enter_cmd pd_dfp_get_enter_mode(int port)
{
	return dfp_enter_mode[port];
}

const uint32_t vdo_tbt_modes[1] =  {
	VDO_MODE_TBT_DEV(0,      /* no vendor specific B1 supported */
			 0,      /* no vendor specific B0 supported */
			 1,      /* Intel specific B0 supported */
			 0,      /* TBT3 adapter */
			 0x0001) /* TBT mode */
};

uint32_t vdo_idh = VDO_IDH(1, /* Data caps as USB host */
			   1, /* Data caps as USB device */
			   IDH_PTYPE_HUB, /* 1: Hub */
			   1, /* Supports alt modes */
			   USB_VID_GOOGLE) |
			   (((0x2) & 0x7) << 23); /* PDUSB host */

const uint32_t vdo_product = VDO_PRODUCT(CONFIG_USB_PID, 0);

const uint32_t vdo_ufp1 = VDO_UFP1(
			0xd, /* USB2.0, USB3.2, and USB4 device capable */
			0x3, /* TBT3, reconfig */
			0x3  /* USB4 Gen3 */);

const uint32_t vdo_dfp = VDO_DFP(7, /* USB2.0, USB3.2 and USB4 host capable */
				 1  /* Port 1 */);

__overridable int svdm_tbt_compat_response_identity(int port, uint32_t *payload)
{
	payload[VDO_I(IDH)] = vdo_idh;
	payload[VDO_I(CSTAT)] = VDO_CSTAT(0);
	payload[VDO_I(PRODUCT)] = vdo_product;
	payload[VDO_I(PTYPE_UFP1_VDO)] = vdo_ufp1;
	payload[VDO_I(PTYPE_UFP2_VDO)] = 0;
	payload[VDO_I(PTYPE_DFP_VDO)] = vdo_dfp;
	return VDO_I(PTYPE_DFP_VDO) + 1;
}


__overridable int svdm_tbt_compat_response_svids(int port, uint32_t *payload)
{
	payload[1] = VDO_SVID(USB_VID_INTEL, 0);
	return 2;
}

__overridable int svdm_tbt_compat_response_modes(int port, uint32_t *payload)
{
	if (PD_VDO_VID(payload[0]) == USB_VID_INTEL) {
		memcpy(payload + 1, vdo_tbt_modes, sizeof(vdo_tbt_modes));
		return ARRAY_SIZE(vdo_tbt_modes) + 1;
	} else {
		return 0; /* NAK */
	}
}

__overridable int svdm_tbt_compat_response_enter_mode(
	int port, uint32_t *payload)
{
	mux_state_t mux_state;

	if ((PD_VDO_VID(payload[0]) == USB_VID_INTEL) &&
		(PD_VDO_OPOS(payload[0]) == OPOS_TBT)) {

		/* Save TBT3 SOP VDO from enter_mode request */
		dfp_enter_mode[port] = (union tbt_dev_mode_enter_cmd)payload[1];
		mux_state = usb_mux_get(port);

		/*
		 * Ref: Figure 6-21 Successful Enter Mode sequence
		 * UFP(responder) should be in USB mode or safe mode before
		 * sending Enter Mode Command response.
		 */
		if ((mux_state & USB_PD_MUX_USB_ENABLED) ||
			(mux_state & USB_PD_MUX_SAFE_MODE)) {

			/*
			 * TODO(b/166455363): Object position in Enter Mode
			 * Command response VDM header is the offset of mode
			 * entered, make sure it's set correctly in response
			 * message buffer.
			 */
			set_tbt_compat_mode_ready(port);
			CPRINTS("UFP Enter TBT mode");
			return 1;
		}
	}

	CPRINTS("UFP failed to enter TBT mode(mux=0x%x)", mux_state);
	return 0; /* NAK */
}

const struct svdm_response svdm_rsp = {
	.identity = &svdm_tbt_compat_response_identity,
	.svids = &svdm_tbt_compat_response_svids,
	.modes = &svdm_tbt_compat_response_modes,
	.enter_mode = &svdm_tbt_compat_response_enter_mode,
	.amode = NULL,
	.exit_mode = NULL,
};
