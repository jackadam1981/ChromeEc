/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Thunderbolt alternate mode support
 * Refer to USB Type-C Cable and Connector Specification Release 2.0 Section F
 */

#include "compile_time_macros.h"
#include "console.h"
#include "tcpm.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_pd_tbt.h"
#include "usb_pe_sm.h"
#include "usb_tbt_alt_mode.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

/* The next VDM command to send for Thunderbolt setup */
static int next_vdm_cmd[CONFIG_USB_PD_PORT_MAX_COUNT];

void tbt_init(int port)
{
	next_vdm_cmd[port] = CMD_ENTER_MODE;
}

void intel_vdm_acked(int port, enum tcpm_transmit_type type, int vdo_count,
		uint32_t *vdm)
{
	const uint8_t vdm_cmd = PD_VDO_CMD(vdm[0]);

	/*
	 * Handle the ACK of a request to exit alt mode.
	 */
	if (type == TCPC_TX_SOP && vdm_cmd == CMD_EXIT_MODE) {
		dpm_init(port);
		return;
	}

	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	    type != TCPC_TX_SOP) {
		print_unexpected_dpm_response(port, type, CMDT_RSP_ACK,
					vdm_cmd, USB_VID_INTEL);
		dpm_set_mode_entry_done(port);
		return;
	}

	if (type == TCPC_TX_SOP) {
		switch (vdm_cmd) {
		case CMD_ENTER_MODE:
			set_tbt_compat_mode_ready(port);
			dpm_set_mode_entry_done(port);
			break;
		default:
			/* This should never happen */
			assert(false);
		}
	}
}

void intel_vdm_naked(int port, enum tcpm_transmit_type type, uint8_t vdm_cmd)
{
	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	    type != TCPC_TX_SOP) {
		print_unexpected_dpm_response(port, type, CMDT_RSP_NAK,
					vdm_cmd, USB_VID_INTEL);
		return;
	}

	dpm_set_mode_entry_done(port);
}

int tbt_setup_next_vdm(int port, int vdo_count, uint32_t *vdm)
{
	const struct pd_discovery *disc =
			pd_get_am_discovery(port, TCPC_TX_SOP);

	if (vdo_count < VDO_MAX_SIZE ||
	    !disc->identity.idh.modal_support ||
	    !is_tbt_cable_superspeed(port)) {
		return 0;
	}

	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE)
		return enter_tbt_compat_mode(port, TCPC_TX_SOP, vdm);

	/* TODO: Add support for Thunderbolt active cable */
	return 0;
}
