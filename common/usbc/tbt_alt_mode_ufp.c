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

const uint32_t vdo_tbt_modes[1] = {
	VDO_MODE_TBT_DEV(0,      /* no vendor specific B1 supported */
			 0,      /* no vendor specific B0 supported */
			 1,      /* Intel specific B0 supported */
			 0,      /* TBT3 adapter */
			 0x0001) /* TBT mode */
};

#ifdef CONFIG_USB_PD_TBT_UFP_DEFAULT_VDO
/* To override those value, redefine macro in board.h */
#define TBT_UFP_IDENTITY_VDO_IDH			\
	(VDO_IDH(1, /* Data caps as USB host */		\
		1, /* Data caps as USB device */	\
		IDH_PTYPE_HUB, /* 1: Hub */		\
		1, /* Supports alt modes */		\
		USB_VID_GOOGLE) |			\
		(((0x2) & 0x7) << 23)) /* PDUSB host */

#define TBT_UFP_IDENTITY_VDO_CSTAT VDO_CSTAT(0)
#define TBT_UFP_IDENTITY_VDO_PRODUCT VDO_PRODUCT(CONFIG_USB_PID, 0)

#define TBT_UFP_IDENTITY_VDO_UFP1					\
	VDO_UFP1(/*0xd, USB2.0, USB3.2, and USB4 device capable */	\
			0x5, /* TODO: change back to 1101b for USB4 */	\
			0x3, /* TBT3, reconfig */			\
			0x3  /* USB4 Gen3 */)


#define TBT_UFP_IDENTITY_VDO_DFP				\
	VDO_DFP(7, /* USB2.0, USB3.2 and USB4 host capable */	\
		1  /* Port 1 */)

#define TBT_UFP_IDENTITY_VDO_UFP2 0
#endif

static int svdm_tbt_compat_response_identity(int port, uint32_t *payload)
{
	payload[VDO_I(IDH)] = TBT_UFP_IDENTITY_VDO_IDH;
	payload[VDO_I(CSTAT)] = TBT_UFP_IDENTITY_VDO_CSTAT;
	payload[VDO_I(PRODUCT)] = TBT_UFP_IDENTITY_VDO_PRODUCT;
	payload[VDO_I(PTYPE_UFP1_VDO)] = TBT_UFP_IDENTITY_VDO_UFP1;
	payload[VDO_I(PTYPE_UFP2_VDO)] = TBT_UFP_IDENTITY_VDO_UFP2;
	payload[VDO_I(PTYPE_DFP_VDO)] = TBT_UFP_IDENTITY_VDO_DFP;
	return VDO_I(PTYPE_DFP_VDO) + 1;
}


static int svdm_tbt_compat_response_svids(int port, uint32_t *payload)
{
	payload[1] = VDO_SVID(USB_VID_INTEL, 0);
	return 2;
}

static int svdm_tbt_compat_response_modes(int port, uint32_t *payload)
{
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

static int svdm_exit_mode(int port, uint32_t *payload)
{
	if ((PD_VDO_VID(payload[0]) == USB_VID_INTEL) &&
		(PD_VDO_OPOS(payload[0]) == OPOS_TBT)) {
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
