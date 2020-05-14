/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* DisplayPort alternate mode support */

#include "assert.h"
#include "stdbool.h"
#include "usb_pd.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

/* The next VDM command to send for DP setup */
static int next_vdm_cmd[CONFIG_USB_PD_PORT_MAX_COUNT];

void dp_init(int port)
{
	dp_reset_next_command(port);
}

void dp_vdm_cmd_acked(int port, int cmd)
{
	if (next_vdm_cmd[port] != cmd) {
		CPRINTF("C%d: Received unexpected VDM ACK for command %d\n",
				port, cmd);
		/*
		 * TODO: This should probably trigger some kind of error
		 * handling behavior.
		 */
		return;
	}

	switch (cmd) {
	case CMD_ENTER_MODE:
		next_vdm_cmd[port] = CMD_DP_STATUS;
		break;
	case CMD_DP_STATUS:
		next_vdm_cmd[port] = CMD_DP_CONFIG;
		break;
	case CMD_DP_CONFIG:
		break;
	default:
		/* This should never happen */
		assert(false);
	}
}

void dp_reset_next_command(int port)
{
	CPRINTF("C%d: Resetting next command to Enter Mode\n", port);
	next_vdm_cmd[port] = CMD_ENTER_MODE;
}

bool dp_setup_next_vdm(int port, uint32_t *vdm, uint32_t *vdo_cnt)
{
	struct svdm_amode_data *modep =
				pd_get_amode_data(port, USB_SID_DISPLAYPORT);
	switch (next_vdm_cmd[port]) {
	case CMD_ENTER_MODE:
		/* Enter the first supported mode for DisplayPort. */
		vdm[0] = pd_dfp_enter_mode(port, USB_SID_DISPLAYPORT, 0);
		/* TODO: pd_dfp_enter_mode should handle this stuff. */
		/* XXX: CMDT_INIT is 0, so this is a no-op */
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP));
		*vdo_cnt = 1;
		break;
	case CMD_DP_STATUS:
		if (!(modep && modep->opos))
			return false;

		*vdo_cnt = modep->fx->status(port, vdm);
		vdm[0] |= PD_VDO_OPOS(modep->opos);
		break;
	case CMD_DP_CONFIG:
		if (!(modep && modep->opos))
			return false;

		*vdo_cnt = modep->fx->config(port, vdm);
		break;
	default:
		CPRINTF("%s called with invalid next VDM command %d\n",
				__func__, next_vdm_cmd[port]);
		return false;
	}
	return true;
}
