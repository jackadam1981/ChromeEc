/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB4 mode support
 * Refer USB Type-C Cable and Connector Specification Release 2.0 Section 5 and
 * USB Power Delivery Specification Revision 3.0, Version 2.0 Section 6.4.8
 */

#include <stdbool.h>
#include <stdint.h>
#include "compile_time_macros.h"
#include "console.h"
#include "tcpm.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_pd_tbt.h"
#include "usb_pe_sm.h"
#include "usb_usb4_alt_mode.h"
#include "usbc_ppc.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

static int usb4_prints(const char *string, int port)
{
	return CPRINTS("C%d: USB4 %s", port, string);
}

enum usb4_states {
	USB4_START = 0,
	USB4_ENTER_SOP,
	USB4_INACTIVE,
	TBT_STATE_COUNT,
};
static enum usb4_states usb4_state[CONFIG_USB_PD_PORT_MAX_COUNT];

void usb4_init(int port)
{
	usb4_state[port] = USB4_START;
}

void usb4_entry_failed(int port)
{
	/*
	 * Since Enter USB sets the mux state to SAFE mode, fall back
	 * to USB mode on receiving a NAK.
	 */
	usb_mux_set(port, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT,
		    pd_get_polarity(port));

	usb4_prints("alt mode protocol failed!", port);
	usb4_state[port] = USB4_INACTIVE;
}

static bool usb4_response_valid(int port, enum tcpm_transmit_type type)
{
	/*
	 * Check for an unexpected response.
	 */
	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	     type != TCPC_TX_SOP) {
		usb4_entry_failed(port);
		return false;
	}
	return true;
}

bool pd_is_usb4_mode_capable(int port)
{
	const struct pd_discovery *disc =
			pd_get_am_discovery(port, TCPC_TX_SOP);
	/*
	 * TODO: b/156749387 Add support for entering the USB4 mode with an
	 * active cable.
	 */
	if (!IS_ENABLED(CONFIG_USBC_SS_MUX) ||
	    !IS_ENABLED(CONFIG_USB_PD_USB4) ||
	    !IS_ENABLED(CONFIG_USB_PD_ALT_MODE_DFP) ||
	    !PD_PRODUCT_IS_USB4(disc->identity.product_t1.raw_value) ||
	    !pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP, USB_VID_INTEL) ||
	    get_usb4_cable_speed(port) < USB_R30_SS_U32_U40_GEN1 ||
	    get_usb_pd_cable_type(port) != IDH_PTYPE_PCABLE)
		return false;

	return true;
}

void enter_usb_acked(int port, enum tcpm_transmit_type type)
{
	if (!usb4_response_valid(port, type))
		return;

	switch (usb4_state[port]) {
	case USB4_ENTER_SOP:
		/* Connect the SBU and USB lines to the connector */
		if (IS_ENABLED(CONFIG_USBC_PPC_SBU))
			ppc_set_sbu(port, 1);

		/* Set usb mux to USB4 mode */
		usb_mux_set(port, USB_PD_MUX_USB4_ENABLED, USB_SWITCH_CONNECT,
			    pd_get_polarity(port));

		usb4_prints("Enter USB SOP ACKed", port);
		break;
	default:
		usb_mux_set(port, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT,
			    pd_get_polarity(port));
		/* Invalid or unexpected negotiation state */
		 CPRINTF("%s called with invalid state %d\n",
				__func__, usb4_state[port]);
	}
}

void enter_usb_naked(int port, enum tcpm_transmit_type type)
{
	if (!usb4_response_valid(port, type))
		return;

	switch (usb4_state[port]) {
	case USB4_ENTER_SOP:
		usb4_prints("Enter USB SOP NAKed", port);
		break;
	default:
		/* Invalid or unexpected negotiation state */
		 CPRINTF("%s called with invalid state %d\n",
				__func__, usb4_state[port]);
	}
	usb4_entry_failed(port);
}

uint32_t enter_usb_setup_next_msg(int port)
{
	switch (usb4_state[port]) {
	case USB4_START:
	case USB4_ENTER_SOP:
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) {
			usb4_state[port] = USB4_ENTER_SOP;
			usb4_prints("Enter USB SOP", port);
			return  get_enter_usb_msg_payload(port);
		}
	default:
		break;
	}
	return 0;
}
