/* Copyright 2020 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * DisplayPort alternate mode support
 * Refer to VESA DisplayPort Alt Mode on USB Type-C Standard, version 2.0,
 * section 5.2
 */

#include <stdbool.h>
#include <stdint.h>
#include "atomic.h"
#include "builtin/assert.h"
#include "chipset.h"
#include "console.h"
#include "power_button.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ##args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ##args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

/* The state of the DP negotiation */
enum dp_states {
	DP_START = 0,
	DP_ENTER_ACKED,
	DP_ENTER_NAKED,
	DP_STATUS_ACKED,
	DP_PREPARE_CONFIG,
	DP_ACTIVE,
	DP_ENTER_RETRY,
	DP_PREPARE_EXIT,
	DP_INACTIVE,
	DP_STATE_COUNT
};
static enum dp_states dp_state[CONFIG_USB_PD_PORT_MAX_COUNT];

/*
 * Map of states to expected VDM commands in responses.
 * Default of 0 indicates no command expected.
 */
static const uint8_t state_vdm_cmd[DP_STATE_COUNT] = {
	[DP_START] = CMD_ENTER_MODE,	     [DP_ENTER_ACKED] = CMD_DP_STATUS,
	[DP_PREPARE_CONFIG] = CMD_DP_CONFIG, [DP_PREPARE_EXIT] = CMD_EXIT_MODE,
	[DP_ENTER_RETRY] = CMD_ENTER_MODE,
};

#ifdef USB_VID_DOCKING_MONITOR
/* The state of the DP negotiation */
enum docking_monitor_vdm_states {
	DOCKING_MONITOR_VDM_START = 0,
	DOCKING_MONITOR_VDM_WAIT_ATTENTION,
	DOCKING_MONITOR_VDM_GET_CONFIG,
	DOCKING_MONITOR_VDM_DONE,
	DOCKING_MONITOR_VDM_STATE_COUNT
};
static enum docking_monitor_vdm_states
	docking_monitor_vdm_state[CONFIG_USB_PD_PORT_MAX_COUNT];
#endif
/*
 * Track if we're retrying due to an Enter Mode NAK
 */
#define DP_FLAG_RETRY BIT(0)

static atomic_t dpm_dp_flags[CONFIG_USB_PD_PORT_MAX_COUNT];

#define DP_SET_FLAG(port, flag) atomic_or(&dpm_dp_flags[port], (flag))
#define DP_CLR_FLAG(port, flag) atomic_clear_bits(&dpm_dp_flags[port], (flag))
#define DP_CHK_FLAG(port, flag) (dpm_dp_flags[port] & (flag))

bool dp_is_active(int port)
{
	return dp_state[port] == DP_ACTIVE || dp_state[port] == DP_PREPARE_EXIT;
}

bool dp_is_idle(int port)
{
	return dp_state[port] == DP_INACTIVE || dp_state[port] == DP_START;
}

void dp_init(int port)
{
	dp_state[port] = DP_START;
	dpm_dp_flags[port] = 0;
}

bool dp_entry_is_done(int port)
{
	return dp_state[port] == DP_ACTIVE || dp_state[port] == DP_INACTIVE;
}

#ifdef USB_VID_DOCKING_MONITOR
void docking_monitor_mode_init(int port)
{
	docking_monitor_vdm_state[port] = DOCKING_MONITOR_VDM_START;
}

bool docking_monitor_mode_entry_is_done(int port)
{
	return docking_monitor_vdm_state[port] == DOCKING_MONITOR_VDM_DONE ||
	       docking_monitor_vdm_state[port] ==
		       DOCKING_MONITOR_VDM_WAIT_ATTENTION;
}
#endif

static void dp_entry_failed(int port)
{
	CPRINTS("C%d: DP alt mode protocol failed!", port);
	dp_state[port] = DP_INACTIVE;
	dpm_dp_flags[port] = 0;
}

static bool dp_response_valid(int port, enum tcpci_msg_type type, char *cmdt,
			      int vdm_cmd)
{
	enum dp_states st = dp_state[port];

	/*
	 * Check for an unexpected response.
	 * If DP is inactive, ignore the command.
	 */
	if (type != TCPCI_MSG_SOP ||
	    (st != DP_INACTIVE && state_vdm_cmd[st] != vdm_cmd)) {
		CPRINTS("C%d: Received unexpected DP VDM %s (cmd %d) from"
			" %s in state %d",
			port, cmdt, vdm_cmd,
			type == TCPCI_MSG_SOP ? "port partner" : "cable plug",
			st);
		dp_entry_failed(port);
		return false;
	}
	return true;
}

static void dp_exit_to_usb_mode(int port)
{
	int opos = pd_alt_mode(port, TCPCI_MSG_SOP, USB_SID_DISPLAYPORT);

	pd_dfp_exit_mode(port, TCPCI_MSG_SOP, USB_SID_DISPLAYPORT, opos);
	set_usb_mux_with_current_data_role(port);

	CPRINTS("C%d: Exited DP mode", port);
	/*
	 * If the EC exits an alt mode autonomously, don't try to enter it
	 * again. If the AP commands the EC to exit DP mode, it might command
	 * the EC to enter again later, so leave the state machine ready for
	 * that possibility.
	 */
	dp_state[port] = DP_INACTIVE;
}

#ifdef USB_VID_DOCKING_MONITOR
void docking_monitor_vdm_acked(int port, enum tcpci_msg_type type,
			       int vdo_count, uint32_t *vdm)
{
	const uint8_t vdm_cmd = PD_VDO_CMD(vdm[0]);

	if (!dp_response_valid(port, type, "ACK", vdm_cmd))
		return;
	switch (docking_monitor_vdm_state[port]) {
	case DOCKING_MONITOR_VDM_START:
		docking_monitor_vdm_state[port] =
			DOCKING_MONITOR_VDM_WAIT_ATTENTION;
		CPRINTS("C%d: Entered docking monitor mode", port);
		break;
	case DOCKING_MONITOR_VDM_GET_CONFIG:
		docking_monitor_vdm_state[port] = DOCKING_MONITOR_VDM_DONE;
		switch ((vdm[1] & MONITOR_STATUS_MASK) >> 3) {
		case MONITOR_STATUS_OFF_TO_ON:
			ccprints("monitor off to on");
			if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
				chipset_power_on();
			else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND)) {
				power_button_pch_press();
				msleep(200);
				power_button_pch_release();
			}
			break;
		case MONITOR_STATUS_STANDBY_TO_OFF:
			ccprints("monitor standby to off");
			break;
		case MONITOR_STATUS_ON_TO_OFF:
			ccprints("monitor on to off");
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void docking_monitor_mode_attention(int port, uint32_t *payload)
{
	docking_monitor_vdm_state[port] = DOCKING_MONITOR_VDM_GET_CONFIG;
}
#endif

void dp_vdm_acked(int port, enum tcpci_msg_type type, int vdo_count,
		  uint32_t *vdm)
{
	const struct svdm_amode_data *modep =
		pd_get_amode_data(port, type, USB_SID_DISPLAYPORT);
	const uint8_t vdm_cmd = PD_VDO_CMD(vdm[0]);

	if (!dp_response_valid(port, type, "ACK", vdm_cmd))
		return;

	/* TODO(b/155890173): Validate VDO count for specific commands */
	switch (dp_state[port]) {
	case DP_START:
	case DP_ENTER_RETRY:
		dp_state[port] = DP_ENTER_ACKED;
		/* Inform PE layer that alt mode is now active */
		pd_set_dfp_enter_mode_flag(port, true);
		break;
	case DP_ENTER_ACKED:
		/* DP status response & UFP's DP attention have same payload. */
		dfp_consume_attention(port, vdm);
		dp_state[port] = DP_STATUS_ACKED;
		break;
	case DP_PREPARE_CONFIG:
		if (modep && modep->opos && modep->fx->post_config)
			modep->fx->post_config(port);
		dp_state[port] = DP_ACTIVE;
		CPRINTS("C%d: Entered DP mode", port);
		break;
	case DP_PREPARE_EXIT:
		/*
		 * Request to exit mode successful, so put the module in an
		 * inactive state or give entry another shot.
		 */
		if (DP_CHK_FLAG(port, DP_FLAG_RETRY)) {
			dp_state[port] = DP_ENTER_RETRY;
			DP_CLR_FLAG(port, DP_FLAG_RETRY);
		} else {
			dp_exit_to_usb_mode(port);
		}
		break;
	case DP_INACTIVE:
		/*
		 * This can occur if the mode is shutdown because
		 * the CPU is being turned off, and an exit mode
		 * command has been sent.
		 */
		break;
	default:
		/* Invalid or unexpected negotiation state */
		CPRINTF("%s called with invalid state %d\n", __func__,
			dp_state[port]);
		dp_entry_failed(port);
		break;
	}
}

void dp_vdm_naked(int port, enum tcpci_msg_type type, uint8_t vdm_cmd)
{
	if (!dp_response_valid(port, type, "NAK", vdm_cmd))
		return;

	switch (dp_state[port]) {
	case DP_START:
		/*
		 * If a request to enter DP mode is NAK'ed, this likely
		 * means the partner is already in DP alt mode, so
		 * request to exit the mode first before retrying
		 * the enter command. This can happen if the EC
		 * is restarted (e.g to go into recovery mode) while
		 * DP alt mode is active.
		 */
		dp_state[port] = DP_ENTER_NAKED;
		break;
	case DP_ENTER_RETRY:
		/*
		 * Another NAK on the second attempt to enter DP mode.
		 * Give up.
		 */
		dp_entry_failed(port);
		break;
	case DP_PREPARE_EXIT:
		/* Treat an Exit Mode NAK the same as an Exit Mode ACK. */
		dp_exit_to_usb_mode(port);
		break;
	default:
		CPRINTS("C%d: NAK for cmd %d in state %d", port, vdm_cmd,
			dp_state[port]);
		dp_entry_failed(port);
		break;
	}
}
#ifdef USB_VID_DOCKING_MONITOR
enum dpm_msg_setup_status
docking_monitor_setup_next_vdm(int port, int *vdo_count, uint32_t *vdm)
{
	int vdo_count_ret = 0;

	if (*vdo_count < VDO_MAX_SIZE)
		return MSG_SETUP_ERROR;

	switch (docking_monitor_vdm_state[port]) {
	case DOCKING_MONITOR_VDM_START:
		vdm[0] = pd_dfp_enter_mode(port, TCPCI_MSG_SOP,
					   USB_VID_DOCKING_MONITOR, 0);
		if (vdm[0] == 0)
			return MSG_SETUP_ERROR;
		/* CMDT_INIT is 0, so this is a no-op */
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		vdo_count_ret = 1;
		CPRINTS("C%d: Attempting to enter docking monitor mode", port);
		break;
	case DOCKING_MONITOR_VDM_GET_CONFIG:
		vdm[0] = VDO(USB_VID_DOCKING_MONITOR, 1,
			     CMD_DOCKING_MONITOR_CONFIG);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		if (chipset_in_state(CHIPSET_STATE_ON))
			/* S0 */
			vdm[1] |= SYSTEM_STATUS_ON;
		else if (chipset_in_state(CHIPSET_STATE_SOFT_OFF))
			/* S5 */
			vdm[1] |= SYSTEM_STATUS_S5;
		else if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
			/* G3 */
			vdm[1] |= SYSTEM_STATUS_G3;
		else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			/* S3 */
			vdm[1] |= SYSTEM_STATUS_SUSPEND;
		vdo_count_ret = 2;
		break;
	default:
		break;
	}

	if (vdo_count_ret) {
		*vdo_count = vdo_count_ret;
		return MSG_SETUP_SUCCESS;
	}
	return MSG_SETUP_UNSUPPORTED;
}
#endif
enum dpm_msg_setup_status dp_setup_next_vdm(int port, int *vdo_count,
					    uint32_t *vdm)
{
	const struct svdm_amode_data *modep =
		pd_get_amode_data(port, TCPCI_MSG_SOP, USB_SID_DISPLAYPORT);
	int vdo_count_ret;

	if (*vdo_count < VDO_MAX_SIZE)
		return MSG_SETUP_ERROR;

	switch (dp_state[port]) {
	case DP_START:
	case DP_ENTER_RETRY:
		/* Enter the first supported mode for DisplayPort. */
		vdm[0] = pd_dfp_enter_mode(port, TCPCI_MSG_SOP,
					   USB_SID_DISPLAYPORT, 0);
		if (vdm[0] == 0)
			return MSG_SETUP_ERROR;
		/* CMDT_INIT is 0, so this is a no-op */
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		vdo_count_ret = 1;
		if (dp_state[port] == DP_START)
			CPRINTS("C%d: Attempting to enter DP mode", port);
		break;
	case DP_ENTER_ACKED:
		if (!(modep && modep->opos))
			return MSG_SETUP_ERROR;

		vdo_count_ret = modep->fx->status(port, vdm);
		if (vdo_count_ret == 0)
			return MSG_SETUP_ERROR;
		vdm[0] |= PD_VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		break;
	case DP_STATUS_ACKED:
		if (!(modep && modep->opos))
			return MSG_SETUP_ERROR;

		if (!get_dp_pin_mode(port))
			return MSG_SETUP_ERROR;

		dp_state[port] = DP_PREPARE_CONFIG;

		/*
		 * Place the USB Type-C pins that are to be re-configured to
		 * DisplayPort Configuration into the Safe state. For
		 * USB_PD_MUX_DOCK, the superspeed signals can remain
		 * connected. For USB_PD_MUX_DP_ENABLED, disconnect the
		 * superspeed signals here, before the pins are re-configured
		 * to DisplayPort (in svdm_dp_post_config, when we receive
		 * the config ack).
		 */
		if (svdm_dp_get_mux_mode(port) == USB_PD_MUX_DP_ENABLED) {
			usb_mux_set_safe_mode(port);
			return MSG_SETUP_MUX_WAIT;
		}
		/* Fall through if no mux set is needed */
	case DP_PREPARE_CONFIG:
		vdo_count_ret = modep->fx->config(port, vdm);
		if (vdo_count_ret == 0)
			return MSG_SETUP_ERROR;
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		break;
	case DP_ENTER_NAKED:
		DP_SET_FLAG(port, DP_FLAG_RETRY);
		/* Fall through to send exit mode */
	case DP_ACTIVE:
		/*
		 * Called to exit DP alt mode, either when the mode
		 * is active and the system is shutting down, or
		 * when an initial request to enter the mode is NAK'ed.
		 * This can happen if the EC is restarted (e.g to go
		 * into recovery mode) while DP alt mode is active.
		 * It would be good to invoke modep->fx->exit but
		 * this doesn't set up the VDM, it clears state.
		 * TODO(b/159856063): Clean up the API to the fx functions.
		 */
		if (!(modep && modep->opos))
			return MSG_SETUP_ERROR;

		usb_mux_set_safe_mode_exit(port);
		dp_state[port] = DP_PREPARE_EXIT;
		return MSG_SETUP_MUX_WAIT;
	case DP_PREPARE_EXIT:
		/* DPM should call setup only after safe state is set */
		vdm[0] = VDO(USB_SID_DISPLAYPORT, 1, /* structured */
			     CMD_EXIT_MODE);

		vdm[0] |= VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPCI_MSG_SOP));
		vdo_count_ret = 1;
		break;
	case DP_INACTIVE:
		/*
		 * DP mode is inactive.
		 */
		return MSG_SETUP_ERROR;
	default:
		CPRINTF("%s called with invalid state %d\n", __func__,
			dp_state[port]);
		return MSG_SETUP_ERROR;
	}

	if (vdo_count_ret) {
		*vdo_count = vdo_count_ret;
		return MSG_SETUP_SUCCESS;
	}

	return MSG_SETUP_UNSUPPORTED;
}
