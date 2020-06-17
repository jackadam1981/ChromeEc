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
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_mux.h"
#include "usb_pd_tbt.h"
#include "usb_pe_sm.h"
#include "usb_tbt_alt_mode.h"
#include "tcpm.h"

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
	tbt_reset_next_command(port);
}

static void print_unexpected_response(int port, enum tcpm_transmit_type type,
		int vdm_cmd_type, int vdm_cmd)
{
	char *cmdt_str;

	switch (vdm_cmd_type) {
	case CMDT_RSP_ACK:
		cmdt_str = "ACK";
		break;
	case CMDT_RSP_NAK:
		cmdt_str = "NAK";
		break;
	default:
		assert(false);
	}

	CPRINTS("C%d: Received unexpected TBT VDM %s (cmd %d) from %s", port,
			cmdt_str, vdm_cmd,
			type == TCPC_TX_SOP ? "port partner" : "cable plug");
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
	if (type != TCPC_TX_SOP || next_vdm_cmd[port] != vdm_cmd) {
		print_unexpected_response(port, type, CMDT_RSP_NAK, vdm_cmd);
		return;
	}

	dpm_set_mode_entry_done(port);
}

void tbt_reset_next_command(int port)
{
	next_vdm_cmd[port] = CMD_ENTER_MODE;
}

int tbt_setup_next_vdm(int port, int vdo_count, uint32_t *vdm)
{
	const struct pd_discovery *disc =
		pd_get_am_discovery(port, TCPC_TX_SOP);
	int vdo_count_ret;

	if (vdo_count < VDO_MAX_SIZE ||
	    !disc->identity.idh.modal_support ||
	    !is_tbt_cable_superspeed(port)) {
		return -1;
	}

	if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) {
		vdo_count_ret = enter_tbt_compat_mode(port, TCPC_TX_SOP, vdm);
		/*
		 * If Cable does not support Intel SVID, limit Thunderbolt
		 * speed to Passive Gen 2.
		 */
		if (!pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP_PRIME,
						USB_VID_INTEL)) {
			vdm[1] = LIMIT_TBT_SPEED(vdm[1], TBT_SS_U32_GEN1_GEN2);
		}
		return vdo_count_ret;
	}
	return -1;
}
