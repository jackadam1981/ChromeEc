/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Device Policy Manager implementation
 * Refer to USB PD 3.0 spec, version 2.0, sections 8.2 and 8.3
 */

#include "charge_state.h"
#include "compile_time_macros.h"
#include "console.h"
#include "ec_commands.h"
#include "system.h"
#include "task.h"
#include "tcpm.h"
#include "usb_dp_alt_mode.h"
#include "usb_mode.h"
#include "usb_pd.h"
#include "usb_pd_dpm.h"
#include "usb_pe_sm.h"
#include "usb_tbt_alt_mode.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif

static struct {
	uint32_t flags;
} dpm[CONFIG_USB_PD_PORT_MAX_COUNT];

#define DPM_SET_FLAG(port, flag) \
	deprecated_atomic_or(&dpm[(port)].flags, (flag))
#define DPM_CLR_FLAG(port, flag) \
	deprecated_atomic_clear_bits(&dpm[(port)].flags, (flag))
#define DPM_CHK_FLAG(port, flag) (dpm[(port)].flags & (flag))

/* Flags for internal DPM state */
#define DPM_FLAG_MODE_ENTRY_DONE BIT(0)
#define DPM_FLAG_EXIT_REQUEST    BIT(1)
#define DPM_FLAG_ENTER_DP        BIT(2)
#define DPM_FLAG_ENTER_TBT       BIT(3)
#define DPM_FLAG_ENTER_USB4      BIT(4)

enum ec_status pd_request_enter_mode(int port, enum typec_mode mode)
{
	/* Only one enter request may be active at a time. */
	if (mode != TYPEC_MODE_NONE &&
			(DPM_CHK_FLAG(port, DPM_FLAG_ENTER_DP) ||
			 DPM_CHK_FLAG(port, DPM_FLAG_ENTER_TBT) ||
			 DPM_CHK_FLAG(port, DPM_FLAG_ENTER_USB4))) {
		/*
		 * TODO(b/168030639): Notify the AP and add the result to the
		 * status response.
		 */
		return EC_RES_BUSY;
	}

	switch (mode) {
	case TYPEC_MODE_DP:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_DP);
		break;
#ifdef CONFIG_USB_PD_TBT_COMPAT_MODE
	case TYPEC_MODE_TBT:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_TBT);
		break;
#endif /* CONFIG_USB_PD_TBT_COMPAT_MODE */
#ifdef CONFIG_USB_PD_USB4
	case TYPEC_MODE_USB4:
		DPM_SET_FLAG(port, DPM_FLAG_ENTER_USB4);
		break;
#endif
	default:
		return EC_RES_INVALID_PARAM;
	}

	DPM_CLR_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE);
	DPM_CLR_FLAG(port, DPM_FLAG_EXIT_REQUEST);
	CPRINTS("C%d: entering mode with flags 0x%x", port, dpm[port].flags);

	return EC_RES_SUCCESS;
}

void dpm_init(int port)
{
	dpm[port].flags = 0;
}

void dpm_set_mode_entry_done(int port)
{
	DPM_SET_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE);
	DPM_CLR_FLAG(port, DPM_FLAG_ENTER_DP | DPM_FLAG_ENTER_TBT |
			DPM_FLAG_ENTER_USB4);
}

void dpm_set_mode_exit_request(int port)
{
	DPM_SET_FLAG(port, DPM_FLAG_EXIT_REQUEST);
}

static void dpm_clear_mode_exit_request(int port)
{
	DPM_CLR_FLAG(port, DPM_FLAG_EXIT_REQUEST);
}

static bool dpm_mode_entry_requested(int port, enum typec_mode mode)
{
	/* If the AP isn't driving, the EC can do what it wants. */
	if (!IS_ENABLED(CONFIG_HOSTCMD_TYPEC))
		return true;

	switch (mode) {
	case TYPEC_MODE_DP:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_DP);
	case TYPEC_MODE_TBT:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_TBT);
	case TYPEC_MODE_USB4:
		return !!DPM_CHK_FLAG(port, DPM_FLAG_ENTER_USB4);
	default:
		return false;
	}
}

void dpm_vdm_acked(int port, enum tcpm_transmit_type type, int vdo_count,
		uint32_t *vdm)
{
	const uint16_t svid = PD_VDO_VID(vdm[0]);

	assert(vdo_count >= 1);

	switch (svid) {
	case USB_SID_DISPLAYPORT:
		dp_vdm_acked(port, type, vdo_count, vdm);
		break;
	case USB_VID_INTEL:
		if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
			intel_vdm_acked(port, type, vdo_count, vdm);
			break;
		}
	default:
		CPRINTS("C%d: Received unexpected VDM ACK for SVID %d", port,
				svid);
	}
}

void dpm_vdm_naked(int port, enum tcpm_transmit_type type, uint16_t svid,
		uint8_t vdm_cmd)
{
	switch (svid) {
	case USB_SID_DISPLAYPORT:
		dp_vdm_naked(port, type, vdm_cmd);
		break;
	case USB_VID_INTEL:
		if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
			intel_vdm_naked(port, type, vdm_cmd);
			break;
		}
	default:
		CPRINTS("C%d: Received unexpected VDM NAK for SVID %d", port,
				svid);
	}
}

/*
 * The call to this function requests that the PE send one VDM, whichever is
 * next in the mode entry sequence. This only happens if preconditions for mode
 * entry are met.
 */
static void dpm_attempt_mode_entry(int port)
{
	int vdo_count = 0;
	uint32_t vdm[VDO_MAX_SIZE];
	enum tcpm_transmit_type tx_type = TCPC_TX_SOP;

	if (pd_get_data_role(port) != PD_ROLE_DFP)
		return;
	/*
	 * Do not try to enter mode while CPU is off.
	 * CPU transitions (e.g b/158634281) can occur during the discovery
	 * phase or during enter/exit negotiations, and the state
	 * of the modes can get out of sync, causing the attempt to
	 * enter the mode to fail prematurely.
	 */
	if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF))
		return;
	/*
	 * If discovery has not occurred for modes, do not attempt to switch
	 * to alt mode.
	 */
	if (pd_get_svids_discovery(port, TCPC_TX_SOP) != PD_DISC_COMPLETE ||
	    pd_get_modes_discovery(port, TCPC_TX_SOP) != PD_DISC_COMPLETE)
		return;

	/* Check if the device and cable support USB4. */
	if (IS_ENABLED(CONFIG_USB_PD_USB4) && enter_usb_is_capable(port) &&
			dpm_mode_entry_requested(port, TYPEC_MODE_USB4)) {
		pd_dpm_request(port, DPM_REQUEST_ENTER_USB);
		return;
	}

	/* If not, check if they support Thunderbolt alt mode. */
	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	    pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP, USB_VID_INTEL) &&
			dpm_mode_entry_requested(port, TYPEC_MODE_TBT))
		vdo_count = tbt_setup_next_vdm(port,
			ARRAY_SIZE(vdm), vdm, &tx_type);

	/* If not, check if they support DisplayPort alt mode. */
	if (vdo_count == 0 && !DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE) &&
	    pd_is_mode_discovered_for_svid(port, TCPC_TX_SOP,
				USB_SID_DISPLAYPORT) &&
	    dpm_mode_entry_requested(port, TYPEC_MODE_DP))
		vdo_count = dp_setup_next_vdm(port, ARRAY_SIZE(vdm), vdm);

	/*
	 * If the PE didn't discover any supported alternate mode, just mark
	 * setup done and get out of here.
	 */
	if (vdo_count == 0 && !DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE)) {
		CPRINTS("C%d: No supported alt mode discovered", port);
		dpm_set_mode_entry_done(port);
		return;
	}

	if (vdo_count < 0) {
		dpm_set_mode_entry_done(port);
		CPRINTS("C%d: Couldn't construct alt mode VDM", port);
		return;
	}

	/*
	 * TODO(b/155890173): Provide a host command to request that the PE send
	 * an arbitrary VDM via this mechanism.
	 */
	if (!pd_setup_vdm_request(port, tx_type, vdm, vdo_count)) {
		dpm_set_mode_entry_done(port);
		return;
	}

	pd_dpm_request(port, DPM_REQUEST_VDM);
}

static void dpm_attempt_mode_exit(int port)
{
	int opos;
	uint16_t svid;
	uint32_t vdm;

	/* TODO(b/156749387): Support Data Reset for exiting USB4. */
	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	    tbt_is_active(port))
		svid = USB_VID_INTEL;
	else if (dp_is_active(port))
		svid = USB_SID_DISPLAYPORT;
	else {
		/* Clear exit mode request */
		dpm_clear_mode_exit_request(port);
		return;
	}

	/*
	 * TODO(b/148528713): Support cable plug Exit Mode (probably outsource
	 * VDM construction to alt mode modules).
	 */
	opos = pd_alt_mode(port, TCPC_TX_SOP, svid);
	if (opos > 0 && pd_dfp_exit_mode(port, TCPC_TX_SOP, svid, opos)) {
		/*
		 * TODO b/159717794: Delay deleting the data until after the
		 * EXIT_MODE message is has ACKed. Unfortunately the callers
		 * of this function expect the mode to be cleaned up before
		 * return.
		 */
		vdm = VDO(svid, 1, /* Structured */
			  VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP)) |
			  VDO_OPOS(opos) | VDO_CMDT(CMDT_INIT) | CMD_EXIT_MODE);

		if (!pd_setup_vdm_request(port, TCPC_TX_SOP, &vdm, 1)) {
			dpm_clear_mode_exit_request(port);
			return;
		}

		pd_dpm_request(port, DPM_REQUEST_VDM);
	}
}

void dpm_run(int port)
{
	if (DPM_CHK_FLAG(port, DPM_FLAG_EXIT_REQUEST))
		dpm_attempt_mode_exit(port);
	else if (!DPM_CHK_FLAG(port, DPM_FLAG_MODE_ENTRY_DONE))
		dpm_attempt_mode_entry(port);
}
