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
#include "usb_common.h"
#include "usb_mode.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_pd_tbt.h"
#include "usb_pe_sm.h"
#include "usb_tbt_alt_mode.h"
#include "usbc_ppc.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

enum usb4_mode_status {
	USB4_MODE_FAILURE,
	USB4_MODE_SUCCESS,
};

enum usb4_states {
	USB4_START,
	USB4_ENTER_SOP,
	USB4_ENTER_SOP_PRIME,
	USB4_ENTER_SOP_PRIME_PRIME,
	USB4_ACTIVE,
	USB4_INACTIVE,
	USB4_STATE_COUNT,
};

/*
 * USB4 flow for Active cable
 *
 * Structured
 * VDM version
 * (cable revision)-- <2.0 -------->|
 *     |                            |
 *     >=2.0                        |
 *     |                            |
 * VDO version---- <1.3 -------> Modal op? -- N --|
 * (B21:23 of                       |             |
 *  Discover ID SOP'-               y             |
 *  Active cable VDO1)              |             |
 *     |                         TBT SVID? -- N --|
 *     >=1.3                        |             |
 *     |                            y             |
 * Cable USB4 support? - N          |             |
 *     |                 |      Gen4 cable? - N - Skip USB4 mode entry
 *     y           Skip USB4        |
 *     |           mode entry       |
 * Enter USB4                       y
 * (SOP',SOP'',SOP)                 |
 *                                  |
 *         |<---- NAK ----- Enter mode TBT SOP'<---|
 *         |                     |                 |
 *         |                    ACK                |
 *         |                     |                 |
 *         |<---- NAK ----- Enter mode TBT SOP''   |
 *         |                     |                 |
 * Exit TBT mode SOP            ACK                |
 *         |                     |                 |
 *      ACK/NAK            Enter USB4 mode         |
 *         |                     SOP               |
 * Exit TBT mode SOP''                             |
 *         |                                       |
 *      ACK/NAK                                    |
 *         |                                       |
 * Exit TBT mode SOP'                              |
 *         |                                       |
 *      ACK/NAK                                    |
 *         |                                       |
 *         |--------Retry done? ---- N ------------|
 *                     |
 *                     y
 *                     |
 *               Skip USB4 mode entry
 */

static enum usb4_states usb4_state[CONFIG_USB_PD_PORT_MAX_COUNT];

static void usb4_debug_prints(int port, enum usb4_mode_status usb4_status)
{
	CPRINTS("C%d: USB4: State:%d Status:%d", port, usb4_state[port],
		usb4_status);
}

bool enter_usb_entry_is_done(int port)
{
	return usb4_state[port] == USB4_ACTIVE ||
		usb4_state[port] == USB4_INACTIVE;
}

void enter_usb_init(int port)
{
	usb4_state[port] = USB4_START;
}

bool enter_usb_is_active(int port)
{
	return usb4_state[port] != USB4_START ||
	       usb4_state[port] != USB4_INACTIVE;
}

void enter_usb_failed(int port)
{
	/*
	 * Since Enter USB sets the mux state to SAFE mode, fall back
	 * to USB mode on receiving a NAK.
	 */
	usb_mux_set(port, USB_PD_MUX_USB_ENABLED, USB_SWITCH_CONNECT,
		    pd_get_polarity(port));

	usb4_debug_prints(port, USB4_MODE_FAILURE);
	usb4_state[port] = USB4_INACTIVE;
}

static bool enter_usb_response_valid(int port, enum tcpm_transmit_type type)
{
	/*
	 * Check for an unexpected response.
	 */
	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	     type != TCPC_TX_SOP) {
		enter_usb_failed(port);
		return false;
	}
	return true;
}

static bool enter_usb_cable_is_capable(int port)
{
	struct pd_discovery *disc_sop_prime =
		pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);

	return disc_sop_prime->identity.product_t2.a2_rev30.usb_40_support ==
				USB4_SUPPORTED ? true : false;
}

bool enter_usb_is_capable(int port)
{
	const struct pd_discovery *disc =
			pd_get_am_discovery(port, TCPC_TX_SOP);
	struct pd_discovery *disc_sop_prime;

	/* TODO: b/156749387 Add support for LRD cable */

	if (!IS_ENABLED(CONFIG_USB_PD_USB4) ||
	    !PD_PRODUCT_IS_USB4(disc->identity.product_t1.raw_value) ||
	    usb4_state[port] == USB4_INACTIVE ||
	    (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	     get_usb4_cable_speed(port) < USB_R30_SS_U32_U40_GEN1))
		return false;

	disc_sop_prime = pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);
	if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
		union tbt_mode_resp_cable cable_mode_resp = {
			.raw_value =
				pd_get_tbt_mode_vdo(port, TCPC_TX_SOP_PRIME) };

		if (pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME) >= VDM_VER20 &&
		    disc_sop_prime->identity.product_t1.a_rev30.vdo_ver >=
							VDO_VERSION_1_3) {
			/*
			 * For VDM version >= 2.0 and VD0 version is >= 1.3,
			 * do not enter USB4 mode if the cable isn't USB4
			 * capable.
			 */
			if (!enter_usb_cable_is_capable(port))
				return false;
		/*
		 * For VDM version < 2.0 or VD0 version < 1.3, do not enter USB4
		 * mode if cable doesn't support modal operation or it doesn't
		 * support Intel svid or if the cable doesn't have rounded
		 * support or if the cable hesn't entered Thunderbolt mode
		 */
		} else if (!disc->identity.idh.modal_support ||
			   !pd_is_mode_discovered_for_svid(port,
					TCPC_TX_SOP_PRIME, USB_VID_INTEL) ||
			   !tbt_cable_entry_is_done(port) ||
			    cable_mode_resp.tbt_rounded !=
					TBT_GEN3_GEN4_ROUNDED_NON_ROUNDED) {
			return false;
		}
	}
	return true;
}

void enter_usb_accepted(int port, enum tcpm_transmit_type type)
{
	struct pd_discovery *disc;

	if (!enter_usb_response_valid(port, type))
		return;

	switch (usb4_state[port]) {
	case USB4_ENTER_SOP_PRIME:
		disc = pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);
		if (disc->identity.product_t1.a_rev20.sop_p_p)
			usb4_state[port] = USB4_ENTER_SOP_PRIME_PRIME;
		else
			usb4_state[port] = USB4_ENTER_SOP;
		break;
	case USB4_ENTER_SOP_PRIME_PRIME:
		usb4_state[port] = USB4_ENTER_SOP;
		break;
	case USB4_ENTER_SOP:
		/* Connect the SBU and USB lines to the connector */
		if (IS_ENABLED(CONFIG_USBC_PPC_SBU))
			ppc_set_sbu(port, 1);

		usb4_state[port] = USB4_ACTIVE;

		/* Set usb mux to USB4 mode */
		usb_mux_set(port, USB_PD_MUX_USB4_ENABLED, USB_SWITCH_CONNECT,
			    pd_get_polarity(port));

		usb4_debug_prints(port, USB4_MODE_SUCCESS);
		break;
	case USB4_ACTIVE:
		break;
	default:
		enter_usb_failed(port);
	}
}

void enter_usb_rejected(int port, enum tcpm_transmit_type type)
{
	if (!enter_usb_response_valid(port, type) ||
	    usb4_state[port] == USB4_ACTIVE)
		return;

	enter_usb_failed(port);
}

uint32_t enter_usb_setup_next_msg(int port, enum tcpm_transmit_type *type)
{
	switch (usb4_state[port]) {
	case USB4_START:
		if (tbt_cable_entry_is_done(port) ||
		    get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) {
			*type = TCPC_TX_SOP;
			usb4_state[port] = USB4_ENTER_SOP;
			return get_enter_usb_msg_payload(port, *type);
		}
	case USB4_ENTER_SOP_PRIME:
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
			*type = TCPC_TX_SOP_PRIME;
			return get_enter_usb_msg_payload(port, *type);
		}
		break;
	case USB4_ENTER_SOP_PRIME_PRIME:
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
			*type = TCPC_TX_SOP_PRIME_PRIME;
			return get_enter_usb_msg_payload(port, *type);
		}
		break;
	case USB4_ENTER_SOP:
		usb4_state[port] = USB4_ENTER_SOP;
		*type = TCPC_TX_SOP;
		return get_enter_usb_msg_payload(port, *type);
	case USB4_ACTIVE:
		return -1;
	default:
		break;
	}
	return 0;
}
