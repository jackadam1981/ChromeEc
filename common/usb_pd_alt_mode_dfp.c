/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Alternate Mode Downstream Facing Port (DFP) USB-PD module.
 */

#include "charge_manager.h"
#include "console.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usbc_ppc.h"
#include "util.h"

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define CPRINTS(format, args...)
#define CPRINTF(format, args...)
#endif

static int pd_get_mode_idx(int port, uint16_t svid)
{
	int i;
	struct pd_policy *pe = pd_get_am_policy(port);

	for (i = 0; i < PD_AMODE_COUNT; i++) {
		if (pe->amodes[i].fx &&
		    (pe->amodes[i].fx->svid == svid))
			return i;
	}
	return -1;
}

static int pd_allocate_mode(int port, uint16_t svid)
{
	int i, j;
	struct svdm_amode_data *modep;
	int mode_idx = pd_get_mode_idx(port, svid);
	struct pd_policy *pe = pd_get_am_policy(port);

	if (mode_idx != -1)
		return mode_idx;

	/* There's no space to enter another mode */
	if (pe->amode_idx == PD_AMODE_COUNT) {
		CPRINTF("ERR:NO AMODE SPACE\n");
		return -1;
	}

	/* Allocate ...  if SVID == 0 enter default supported policy */
	for (i = 0; i < supported_modes_cnt; i++) {
		for (j = 0; j < pe->svid_cnt; j++) {
			struct svdm_svid_data *svidp = &pe->svids[j];

			if ((svidp->svid != supported_modes[i].svid) ||
			    (svid && (svidp->svid != svid)))
				continue;

			modep = &pe->amodes[pe->amode_idx];
			modep->fx = &supported_modes[i];
			modep->data = &pe->svids[j];
			pe->amode_idx++;
			return pe->amode_idx - 1;
		}
	}
	return -1;
}

static int validate_mode_request(struct svdm_amode_data *modep,
				 uint16_t svid, int opos)
{
	if (!modep->fx)
		return 0;

	if (svid != modep->fx->svid) {
		CPRINTF("ERR:svid r:0x%04x != c:0x%04x\n",
			svid, modep->fx->svid);
		return 0;
	}

	if (opos != modep->opos) {
		CPRINTF("ERR:opos r:%d != c:%d\n",
			opos, modep->opos);
		return 0;
	}

	return 1;
}

static int is_vdo_present(int cnt, int index)
{
	return cnt > index;
}

static void enable_transmit_sop_prime(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP)) {
		struct pd_cable *cable = pd_get_cable_attributes(port);
		cable->flags |= CABLE_FLAGS_SOP_PRIME_ENABLE;
	}
}

static void enable_transmit_sop_prime_prime(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP)) {
		struct pd_cable *cable = pd_get_cable_attributes(port);
		cable->flags |= CABLE_FLAGS_SOP_PRIME_PRIME_ENABLE;
	}
}

static bool is_tbt_compat_enabled(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	return (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	       (cable->flags & CABLE_FLAGS_TBT_COMPAT_ENABLE));
}

static void enable_tbt_compat_mode(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE))
		cable->flags |= CABLE_FLAGS_TBT_COMPAT_ENABLE;
}

static inline void disable_tbt_compat_mode(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE))
		cable->flags &= ~CABLE_FLAGS_TBT_COMPAT_ENABLE;
}

static void set_tbt_compat_mode_ready(int port)
{
	if (IS_ENABLED(CONFIG_USBC_SS_MUX) &&
	    IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
		/* Connect the SBU and USB lines to the connector. */
		if (IS_ENABLED(CONFIG_USBC_PPC_SBU))
			ppc_set_sbu(port, 1);

		/* Set usb mux to Thunderbolt-compatible mode */
		usb_mux_set(port, USB_PD_MUX_TBT_COMPAT_ENABLED,
			USB_SWITCH_CONNECT, pd_get_polarity(port));
	}
}

static bool is_tbt_cable_superspeed(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
	    IS_ENABLED(CONFIG_USB_PD_DECODE_SOP)) {
		struct pd_cable *cable = pd_get_cable_attributes(port);

		/* Product type is Active cable, hence don't check for speed */
		if (cable->type == IDH_PTYPE_ACABLE)
			return true;

		if (cable->type != IDH_PTYPE_PCABLE)
			return false;

		if (IS_ENABLED(CONFIG_USB_PD_REV30) && cable->rev == PD_REV30)
			return cable->attr.p_rev30.ss ==
				USB_R30_SS_U32_U40_GEN1 ||
				cable->attr.p_rev30.ss ==
				USB_R30_SS_U32_U40_GEN2 ||
				cable->attr.p_rev30.ss ==
				USB_R30_SS_U40_GEN3;

		return cable->attr.p_rev20.ss ==
			USB_R20_SS_U31_GEN1 ||
			cable->attr.p_rev20.ss ==
			USB_R20_SS_U31_GEN1_GEN2;
	}

	return false;
}

/* Check if product supports any Modal Operation (Alternate Modes) */
static bool is_modal(int port, int cnt, uint32_t *payload)
{
	return IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE) &&
		is_vdo_present(cnt, VDO_INDEX_IDH) &&
		PD_IDH_IS_MODAL(payload[VDO_INDEX_IDH]);
}

static bool is_intel_svid(int port, int prev_svid_cnt)
{
	/*
	 * Check if SVID0 = USB_VID_INTEL
	 * (Ref: USB Type-C cable and connector specification, Table F-9)
	 */
	if (IS_ENABLED(CONFIG_USB_PD_TBT_COMPAT_MODE)) {
		int i;
		struct pd_policy *pe = pd_get_am_policy(port);

		/*
		 * errata: All the Thunderbolt certified cables and docks
		 * tested have SVID1 = 0x8087
		 *
		 * For the Discover SVIDs, responder may present the SVIDs
		 * in any order hence check all SVIDs if Intel SVID present.
		 */
		for (i = prev_svid_cnt; i < pe->svid_cnt; i++) {
			if (pe->svids[i].svid == USB_VID_INTEL)
				return true;
		}
	}

	return false;
}

static inline bool is_tbt_compat_mode(int port, int cnt, uint32_t *payload)
{
	/*
	 * Ref: USB Type-C cable and connector specification
	 * F.2.5 TBT3 Device Discover Mode Responses
	 */
	return is_vdo_present(cnt, VDO_INDEX_IDH) &&
		PD_VDO_RESP_MODE_INTEL_TBT(payload[VDO_INDEX_IDH]);
}

static inline void limit_tbt_cable_speed(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	/* Cable flags are cleared when cable reset is called */
	cable->flags |= CABLE_FLAGS_TBT_COMPAT_LIMIT_SPEED;
}

static inline bool is_limit_tbt_cable_speed(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	return !!(cable->flags & CABLE_FLAGS_TBT_COMPAT_LIMIT_SPEED);
}

static inline bool is_usb4_mode_enabled(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	return (IS_ENABLED(CONFIG_USB_PD_USB4) &&
	       (cable->flags & CABLE_FLAGS_USB4_CAPABLE));
}

static inline void enable_usb4_mode(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (IS_ENABLED(CONFIG_USB_PD_USB4))
		cable->flags |= CABLE_FLAGS_USB4_CAPABLE;
}

static inline void disable_usb4_mode(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (IS_ENABLED(CONFIG_USB_PD_USB4))
		cable->flags &= ~CABLE_FLAGS_USB4_CAPABLE;
}

static inline void enable_enter_usb4_mode(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (IS_ENABLED(CONFIG_USB_PD_USB4))
		cable->flags |= CABLE_FLAGS_ENTER_USB_MODE;
}

/*
 * Ref: USB Type-C Cable and Connector Specification
 * Figure F-1 TBT3 Discovery Flow
 */
static void dfp_consume_cable_response(int port, int cnt, uint32_t *payload,
					uint16_t head)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	if (cable->is_identified)
		return;

	/* Get cable rev */
	cable->rev = PD_HEADER_REV(head);

	if (is_vdo_present(cnt, VDO_INDEX_IDH)) {
		cable->type = PD_IDH_PTYPE(payload[VDO_INDEX_IDH]);
		if (is_vdo_present(cnt, VDO_INDEX_PTYPE_CABLE1))
			cable->attr.raw_value =
					payload[VDO_INDEX_PTYPE_CABLE1];
	}
	/*
	 * Ref USB PD Spec 3.0  Pg 145. For active cable there are two VDOs.
	 * Hence storing the second VDO.
	 */
	if (IS_ENABLED(CONFIG_USB_PD_REV30) &&
	    is_vdo_present(cnt, VDO_INDEX_PTYPE_CABLE2) &&
	    cable->type == IDH_PTYPE_ACABLE)
		cable->attr2.raw_value = payload[VDO_INDEX_PTYPE_CABLE2];

	cable->is_identified = 1;
}

/*
 * Ref: USB Type-C Cable and Connector Specification
 * Figure 5-1 USB4 Discovery and Entry Flow Model.
 *
 * Note: USB Type-C Cable and Connector Specification
 * doesn't include details for Revision 2 cables.
 *
 *                         Passive Cable
 *                                |
 *                -----------------------------------
 *                |                                 |
 *           Revision 2                        Revision 3
 *          USB Signalling                   USB Signalling
 *             |                                     |
 *     ------------------            -------------------------
 *     |       |        |            |       |       |       |
 * USB2.0   USB3.1    USB3.1       USB3.2   USB4   USB3.2   USB2
 *   |      Gen1      Gen1 Gen2    Gen2     Gen3   Gen1       |
 *   |       |          |           |        |       |       Exit
 *   --------           ------------         --------        USB4
 *      |                    |                  |          Discovery.
 *    Exit          Is DFP Gen3 Capable?     Enter USB4
 *    USB4                  |                with respective
 *   Discovery.   --- No ---|--- Yes ---     cable speed.
 *                |                    |
 *    Enter USB4 with             Is Cable TBT3
 *    respective cable                 |
 *    speed.                 --- No ---|--- Yes ---
 *                           |                    |
 *                   Enter USB4 with        Enter USB4 with
 *                   TBT Gen2 passive       TBT Gen3 passive
 *                   cable.                 cable.
 *
 */
static bool is_cable_ready_to_enter_usb4(int port, int cnt)
{
	/* TODO: USB4 enter mode for Active cables */
	if (IS_ENABLED(CONFIG_USB_PD_USB4) &&
	   (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) &&
	    is_vdo_present(cnt, VDO_INDEX_PTYPE_CABLE1)) {
		struct pd_cable *cable = pd_get_cable_attributes(port);

		switch (cable->rev) {
		case PD_REV30:
			switch (cable->attr.p_rev30.ss) {
			case USB_R30_SS_U40_GEN3:
			case USB_R30_SS_U32_U40_GEN1:
				return true;
			case USB_R30_SS_U32_U40_GEN2:
				/* Check if DFP is Gen 3 capable */
				if (IS_ENABLED(CONFIG_USB_PD_TBT_GEN3_CAPABLE))
					return false;
				return true;
			default:
				disable_usb4_mode(port);
				return false;
			}
		case PD_REV20:
			switch (cable->attr.p_rev20.ss) {
			case USB_R20_SS_U31_GEN1_GEN2:
				/* Check if DFP is Gen 3 capable */
				if (IS_ENABLED(CONFIG_USB_PD_TBT_GEN3_CAPABLE))
					return false;
				return true;
			default:
				disable_usb4_mode(port);
				return false;
		}
		default:
			disable_usb4_mode(port);
		}
	}

	return false;
}

static bool is_usb4_vdo(int port, int cnt, uint32_t *payload)
{
	enum idh_ptype ptype = PD_IDH_PTYPE(payload[VDO_I(PRODUCT)]);

	/*
	 * Product types Hub and peripheral should use UFP product vdos
	 * Reference Table 6-30 USB PD spec 3.2.
	 */
	if (ptype == IDH_PTYPE_HUB || ptype == IDH_PTYPE_PERIPH) {
		/*
		 * Ref: USB Type-C Cable and Connector Specification
		 * Figure 5-1 USB4 Discovery and Entry Flow Model
		 * Device USB4 VDO detection.
		 */
		return IS_ENABLED(CONFIG_USB_PD_USB4) &&
			is_vdo_present(cnt, VDO_INDEX_PTYPE_UFP1_VDO) &&
			PD_PRODUCT_IS_USB4(payload[VDO_INDEX_PTYPE_UFP1_VDO]);
	}

	return false;
}

static int dfp_discover_ident(uint32_t *payload)
{
	payload[0] = VDO(USB_SID_PD, 1, CMD_DISCOVER_IDENT);
	return 1;
}

static int dfp_discover_svids(uint32_t *payload)
{
	payload[0] = VDO(USB_SID_PD, 1, CMD_DISCOVER_SVID);
	return 1;
}

/*
 * This function returns
 * True - If the THunderbolt cable speed is TBT_SS_TBT_GEN3 or
 *        TBT_SS_U32_GEN1_GEN2
 * False - Otherwise
 */
static bool check_tbt_cable_speed(int port)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	return (cable->cable_mode_resp.tbt_cable_speed == TBT_SS_TBT_GEN3 ||
		cable->cable_mode_resp.tbt_cable_speed == TBT_SS_U32_GEN1_GEN2);
}

/*
 * Enter Thunderbolt-compatible mode
 * Reference: USB Type-C cable and connector specification, Release 2.0
 *
 * This function fills the TBT3 objects in the payload and
 * returns the number of objects it has filled.
 */
static int enter_tbt_compat_mode(int port, uint32_t *payload)
{
	union tbt_dev_mode_enter_cmd enter_dev_mode = {0};
	struct pd_cable *cable = pd_get_cable_attributes(port);

	/* Table F-12 TBT3 Cable Enter Mode Command */
	payload[0] = pd_dfp_enter_mode(port, USB_VID_INTEL, 0) |
					VDO_SVDM_VERS(VDM_VER20);

	/* For TBT3 Cable Enter Mode Command, number of Objects is 1 */
	if (is_transmit_msg_sop_prime(port) ||
	    is_transmit_msg_sop_prime_prime(port))
		return 1;

	usb_mux_set_safe_mode(port);

	/* Table F-13 TBT3 Device Enter Mode Command */
	enter_dev_mode.vendor_spec_b1 = cable->dev_mode_resp.vendor_spec_b1;
	enter_dev_mode.vendor_spec_b0 = cable->dev_mode_resp.vendor_spec_b0;
	enter_dev_mode.intel_spec_b0 = cable->dev_mode_resp.intel_spec_b0;
	enter_dev_mode.cable =
		get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE ?
			TBT_ENTER_PASSIVE_CABLE : TBT_ENTER_ACTIVE_CABLE;

	if (cable->cable_mode_resp.tbt_cable_speed == TBT_SS_TBT_GEN3) {
		enter_dev_mode.lsrx_comm = cable->cable_mode_resp.lsrx_comm;
		enter_dev_mode.retimer_type =
					cable->cable_mode_resp.retimer_type;
		enter_dev_mode.tbt_cable = cable->cable_mode_resp.tbt_cable;
		enter_dev_mode.tbt_rounded = cable->cable_mode_resp.tbt_rounded;
		enter_dev_mode.tbt_cable_speed =
					cable->cable_mode_resp.tbt_cable_speed;
	} else {
		enter_dev_mode.tbt_cable_speed = TBT_SS_U32_GEN1_GEN2;
	}
	enter_dev_mode.tbt_alt_mode = TBT_ALTERNATE_MODE;

	payload[1] = enter_dev_mode.raw_value;

	/* For TBT3 Device Enter Mode Command, number of Objects are 2 */
	return 2;
}

static int process_tbt_compat_discover_modes(int port, uint32_t *payload)
{
	int rsize;
	enum tbt_compat_cable_speed max_tbt_speed;
	struct pd_cable *cable = pd_get_cable_attributes(port);
	struct pd_policy *pe = pd_get_am_policy(port);

	/*
	 * For active cables, Enter mode: SOP', SOP'', SOP
	 * Ref: USB Type-C Cable and Connector Specification, figure F-1: TBT3
	 * Discovery Flow and Section F.2.7 TBT3 Cable Enter Mode Command.
	 */
	if (is_transmit_msg_sop_prime(port)) {
		/* Store Discover Mode SOP' response */
		cable->cable_mode_resp.raw_value = payload[1];

		/* Cable does not have Intel SVID for Discover SVID */
		if (is_limit_tbt_cable_speed(port))
			cable->cable_mode_resp.tbt_cable_speed =
						TBT_SS_U32_GEN1_GEN2;

		max_tbt_speed = board_get_max_tbt_speed(port);
		if (cable->cable_mode_resp.tbt_cable_speed > max_tbt_speed)
			cable[port].cable_mode_resp.tbt_cable_speed =
								max_tbt_speed;

		/*
		 * Enter Mode SOP' (Cable Enter Mode) and Enter USB SOP' is
		 * skipped for passive cables.
		 */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE)
			disable_transmit_sop_prime(port);

		if (is_usb4_mode_enabled(port)) {
			/*
			 * If Cable is not Thunderbolt Gen 3
			 * capable or Thunderbolt Gen1_Gen2
			 * capable, disable USB4 mode and
			 * continue flow for
			 * Thunderbolt-compatible mode
			 */
			if (check_tbt_cable_speed(port)) {
				enable_enter_usb4_mode(port);
				usb_mux_set_safe_mode(port);
				return 0;
			}
			disable_usb4_mode(port);
		}
		rsize = enter_tbt_compat_mode(port, payload);
	} else {
		/* Store Discover Mode SOP response */
		cable->dev_mode_resp.raw_value = payload[1];

		if (is_limit_tbt_cable_speed(port)) {
			/*
			 * Passive cable has Nacked for Discover SVID.
			 * No need to do Discover modes of cable. Assign the
			 * cable discovery attributes and enter into device
			 * Thunderbolt-compatible mode.
			 */
			cable->cable_mode_resp.tbt_cable_speed =
				(cable->rev == PD_REV30 &&
				cable->attr.p_rev30.ss >
					USB_R30_SS_U32_U40_GEN2) ?
				TBT_SS_U32_GEN1_GEN2 :
				cable->attr.p_rev30.ss;

			rsize = enter_tbt_compat_mode(port, payload);
		} else {
			/* Discover modes for SOP' */
			pe->svid_idx--;
			rsize = dfp_discover_modes(port, payload);
			enable_transmit_sop_prime(port);
		}
	}

	return rsize;
}

/*
 * This function returns number of objects required to enter
 * Thunderbolt-Compatible mode i.e.
 * 2 - When SOP is enabled.
 * 1 - When SOP' or SOP'' is enabled.
 * 0 - Acknowledge.
 */
static int enter_mode_tbt_compat(int port, uint32_t *payload)
{
	struct pd_cable *cable = pd_get_cable_attributes(port);

	/* Enter mode SOP' for active cables */
	if (is_transmit_msg_sop_prime(port)) {
		disable_transmit_sop_prime(port);
		/* Check if the cable has a SOP'' controller */
		if (cable->attr.a_rev20.sop_p_p)
			enable_transmit_sop_prime_prime(port);
		return enter_tbt_compat_mode(port, payload);
	}

	/* Enter Mode SOP'' for active cables with SOP'' controller */
	if (is_transmit_msg_sop_prime_prime(port)) {
		disable_transmit_sop_prime_prime(port);
		return enter_tbt_compat_mode(port, payload);
	}

	/* Update Mux state to Thunderbolt-compatible mode. */
	set_tbt_compat_mode_ready(port);

	/* No response once device (and cable) acks */
	return 0;
}

/* Return the current cable speed received from Cable Discover Mode command */
/*
 * This algorithm defaults to choosing higher pin config over lower ones in
 * order to prefer multi-function if desired.
 *
 *  NAME | SIGNALING | OUTPUT TYPE | MULTI-FUNCTION | PIN CONFIG
 * -------------------------------------------------------------
 *  A    |  USB G2   |  ?          | no             | 00_0001
 *  B    |  USB G2   |  ?          | yes            | 00_0010
 *  C    |  DP       |  CONVERTED  | no             | 00_0100
 *  D    |  PD       |  CONVERTED  | yes            | 00_1000
 *  E    |  DP       |  DP         | no             | 01_0000
 *  F    |  PD       |  DP         | yes            | 10_0000
 *
 * if UFP has NOT asserted multi-function preferred code masks away B/D/F
 * leaving only A/C/E.  For single-output dongles that should leave only one
 * possible pin config depending on whether its a converter DP->(VGA|HDMI) or DP
 * output.  If UFP is a USB-C receptacle it may assert C/D/E/F.  The DFP USB-C
 * receptacle must always choose C/D in those cases.
 */
int pd_dfp_dp_get_pin_mode(int port, uint32_t status)
{
	struct svdm_amode_data *modep =
				pd_get_amode_data(port, USB_SID_DISPLAYPORT);
	uint32_t mode_caps;
	uint32_t pin_caps;

	if (!modep)
		return 0;

	mode_caps = modep->data->mode_vdo[modep->opos - 1];

	/* TODO(crosbug.com/p/39656) revisit with DFP that can be a sink */
	pin_caps = PD_DP_PIN_CAPS(mode_caps);

	/* if don't want multi-function then ignore those pin configs */
	if (!PD_VDO_DPSTS_MF_PREF(status))
		pin_caps &= ~MODE_DP_PIN_MF_MASK;

	/* TODO(crosbug.com/p/39656) revisit if DFP drives USB Gen 2 signals */
	pin_caps &= ~MODE_DP_PIN_BR2_MASK;

	/* if C/D present they have precedence over E/F for USB-C->USB-C */
	if (pin_caps & (MODE_DP_PIN_C | MODE_DP_PIN_D))
		pin_caps &= ~(MODE_DP_PIN_E | MODE_DP_PIN_F);

	/* get_next_bit returns undefined for zero */
	if (!pin_caps)
		return 0;

	return 1 << get_next_bit(&pin_caps);
}

struct svdm_amode_data *pd_get_amode_data(int port, uint16_t svid)
{
	int idx = pd_get_mode_idx(port, svid);
	struct pd_policy *pe = pd_get_am_policy(port);

	return (idx == -1) ? NULL : &pe->amodes[idx];
}

/*
 * Enter default mode ( payload[0] == 0 ) or attempt to enter mode via svid &
 * opos
 */
uint32_t pd_dfp_enter_mode(int port, uint16_t svid, int opos)
{
	int mode_idx = pd_allocate_mode(port, svid);
	struct pd_policy *pe = pd_get_am_policy(port);
	struct svdm_amode_data *modep;
	uint32_t mode_caps;

	if (mode_idx == -1)
		return 0;
	modep = &pe->amodes[mode_idx];

	if (!opos) {
		/* choose the lowest as default */
		modep->opos = 1;
	} else if (opos <= modep->data->mode_cnt) {
		modep->opos = opos;
	} else {
		CPRINTF("opos error\n");
		return 0;
	}

	mode_caps = modep->data->mode_vdo[modep->opos - 1];
	if (modep->fx->enter(port, mode_caps) == -1)
		return 0;

	pd_set_dfp_enter_mode_flag(port, true);

	/* SVDM to send to UFP for mode entry */
	return VDO(modep->fx->svid, 1, CMD_ENTER_MODE | VDO_OPOS(modep->opos));
}

int pd_dfp_exit_mode(int port, uint16_t svid, int opos)
{
	struct svdm_amode_data *modep;
	struct pd_policy *pe = pd_get_am_policy(port);
	int idx;

	/*
	 * Empty svid signals we should reset DFP VDM state by exiting all
	 * entered modes then clearing state.  This occurs when we've
	 * disconnected or for hard reset.
	 */
	if (!svid) {
		for (idx = 0; idx < PD_AMODE_COUNT; idx++)
			if (pe->amodes[idx].fx)
				pe->amodes[idx].fx->exit(port);

		pd_dfp_pe_init(port);
		return 0;
	}

	/*
	 * TODO(crosbug.com/p/33946) : below needs revisited to allow multiple
	 * mode exit.  Additionally it should honor OPOS == 7 as DFP's request
	 * to exit all modes.  We currently don't have any UFPs that support
	 * multiple modes on one SVID.
	 */
	modep = pd_get_amode_data(port, svid);
	if (!modep || !validate_mode_request(modep, svid, opos))
		return 0;

	/* call DFPs exit function */
	modep->fx->exit(port);

	pd_set_dfp_enter_mode_flag(port, false);

	/* exit the mode */
	modep->opos = 0;
	return 1;
}

void dfp_consume_attention(int port, uint32_t *payload)
{
	uint16_t svid = PD_VDO_VID(payload[0]);
	int opos = PD_VDO_OPOS(payload[0]);
	struct svdm_amode_data *modep = pd_get_amode_data(port, svid);

	if (!modep || !validate_mode_request(modep, svid, opos))
		return;

	if (modep->fx->attention)
		modep->fx->attention(port, payload);
}

void dfp_consume_identity(int port, int cnt, uint32_t *payload)
{
	int ptype = PD_IDH_PTYPE(payload[VDO_I(IDH)]);
	struct pd_policy *pe = pd_get_am_policy(port);
	size_t identity_size = MIN(sizeof(pe->identity),
				   (cnt - 1) * sizeof(uint32_t));
	pd_dfp_pe_init(port);
	memcpy(pe->identity, payload + 1, identity_size);

	switch (ptype) {
	case IDH_PTYPE_AMA:
		/* Leave vbus ON if the following macro is false */
		if (IS_ENABLED(CONFIG_USB_PD_DUAL_ROLE) &&
			IS_ENABLED(CONFIG_USBC_VCONN_SWAP)) {
			/* Adapter is requesting vconn, try to supply it */
			if (PD_VDO_AMA_VCONN_REQ(payload[VDO_I(AMA)]))
				pd_try_vconn_src(port);

			/* Only disable vbus if vconn was requested */
			if (PD_VDO_AMA_VCONN_REQ(payload[VDO_I(AMA)]) &&
				!PD_VDO_AMA_VBUS_REQ(payload[VDO_I(AMA)]))
				pd_power_supply_reset(port);
		}
		break;
	default:
		break;
	}
}

void dfp_consume_svids(int port, int cnt, uint32_t *payload)
{
	int i;
	uint32_t *ptr = payload + 1;
	int vdo = 1;
	uint16_t svid0, svid1;
	struct pd_policy *pe = pd_get_am_policy(port);

	for (i = pe->svid_cnt; i < pe->svid_cnt + 12; i += 2) {
		if (i == SVID_DISCOVERY_MAX) {
			CPRINTF("ERR:SVIDCNT\n");
			break;
		}
		/*
		 * Verify we're still within the valid packet (count will be one
		 * for the VDM header + xVDOs)
		 */
		if (vdo >= cnt)
			break;

		svid0 = PD_VDO_SVID_SVID0(*ptr);
		if (!svid0)
			break;
		pe->svids[i].svid = svid0;
		pe->svid_cnt++;

		svid1 = PD_VDO_SVID_SVID1(*ptr);
		if (!svid1)
			break;
		pe->svids[i + 1].svid = svid1;
		pe->svid_cnt++;
		ptr++;
		vdo++;
	}
	/* TODO(tbroch) need to re-issue discover svids if > 12 */
	if (i && ((i % 12) == 0))
		CPRINTF("ERR:SVID+12\n");
}

void dfp_consume_modes(int port, int cnt, uint32_t *payload)
{
	struct pd_policy *pe = pd_get_am_policy(port);
	int idx = pe->svid_idx;

	pe->svids[idx].mode_cnt = cnt - 1;

	if (pe->svids[idx].mode_cnt < 0) {
		CPRINTF("ERR:NOMODE\n");
	} else {
		memcpy(pe->svids[pe->svid_idx].mode_vdo, &payload[1],
		       sizeof(uint32_t) * pe->svids[idx].mode_cnt);
	}

	pe->svid_idx++;
}

int dfp_discover_modes(int port, uint32_t *payload)
{
	struct pd_policy *pe = pd_get_am_policy(port);
	uint16_t svid = pe->svids[pe->svid_idx].svid;

	if (pe->svid_idx >= pe->svid_cnt)
		return 0;

	payload[0] = VDO(svid, 1, CMD_DISCOVER_MODES);

	return 1;
}

int pd_alt_mode(int port, uint16_t svid)
{
	struct svdm_amode_data *modep = pd_get_amode_data(port, svid);

	return (modep) ? modep->opos : -1;
}

uint16_t pd_get_identity_vid(int port)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return PD_IDH_VID(pe->identity[0]);
}

uint16_t pd_get_identity_pid(int port)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return PD_PRODUCT_PID(pe->identity[2]);
}

uint8_t pd_get_product_type(int port)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return PD_IDH_PTYPE(pe->identity[0]);
}

int pd_get_svid_count(int port)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return pe->svid_cnt;
}

uint16_t pd_get_svid(int port, uint16_t svid_idx)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return pe->svids[svid_idx].svid;
}

uint32_t *pd_get_mode_vdo(int port, uint16_t svid_idx)
{
	struct pd_policy *pe = pd_get_am_policy(port);

	return pe->svids[svid_idx].mode_vdo;
}

__overridable bool board_is_tbt_usb4_port(int port)
{
	return true;
}

int dfp_handle_acked_discover_ident(int port, int cnt, uint32_t *payload,
					uint16_t head)
{
	int rsize;

	/* Received a SOP' Discover Ident msg */
	if (is_transmit_msg_sop_prime(port)) {
		/* Store cable type */
		dfp_consume_cable_response(port, cnt, payload, head);

		/*
		 * Enter USB4 mode if the cable supports USB4
		 * operation and has USB4 VDO.
		 */
		if (is_usb4_mode_enabled(port) &&
		    is_cable_ready_to_enter_usb4(port, cnt)) {
			enable_enter_usb4_mode(port);
			usb_mux_set_safe_mode(port);
			disable_transmit_sop_prime(port);
			/*
			 * To change the mode of operation from USB4 the port
			 * needs to be reconfigured.
			 * Ref: USB Type-C Cable and Connectot
			 * Specification section 5.4.4.
			 */
			disable_tbt_compat_mode(port);
			return 0;
		}

		/*
		 * Disable Thunderbolt-compatible mode if the
		 * cable does not support superspeed
		 */
		if (is_tbt_compat_enabled(port) &&
			!is_tbt_cable_superspeed(port))
			disable_tbt_compat_mode(port);

		rsize = dfp_discover_svids(payload);

		disable_transmit_sop_prime(port);
	/* Received a SOP Discover Ident Message */
	} else if (IS_ENABLED(CONFIG_USB_PD_DECODE_SOP) &&
		board_is_tbt_usb4_port(port)) {
		dfp_consume_identity(port, cnt, payload);

		/*
		 * Enable USB4 mode if USB4 VDO present
		 * and port partner supports USB Rev 3.0.
		 */
		if (is_usb4_vdo(port, cnt, payload) &&
		    PD_HEADER_REV(head)	== PD_REV30)
			enable_usb4_mode(port);

		/*
		 * Enable Thunderbolt-compatible mode
		 * if the modal operation is supported
		 */
		if (is_modal(port, cnt, payload))
			enable_tbt_compat_mode(port);

		if (is_modal(port, cnt, payload) ||
		    is_usb4_vdo(port, cnt, payload)) {
			rsize = dfp_discover_ident(payload);
			enable_transmit_sop_prime(port);
		} else {
			rsize = dfp_discover_svids(payload);
		}
	} else {
		dfp_consume_identity(port, cnt, payload);
		rsize = dfp_discover_svids(payload);
	}

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER) &&
	    pd_charge_from_device(pd_get_identity_vid(port),
				  pd_get_identity_pid(port)))
		charge_manager_update_dualrole(port, CAP_DEDICATED);

	return rsize;
}

int dfp_handle_acked_discover_svid(int port, int cnt, uint32_t *payload)
{
	int rsize;
	struct pd_policy *pe = pd_get_am_policy(port);
	int prev_svid_cnt = pe->svid_cnt;

	dfp_consume_svids(port, cnt, payload);

	/*
	 * Ref: USB Type-C Cable and Connector Specification,
	 * figure F-1: TBT3 Discovery Flow
	 *
	 * Check if 0x8087 is received for Discover SVID SOP.
	 * If not, disable Thunderbolt-compatible mode
	 *
	 * If 0x8087 is not received for Discover SVID SOP'
	 * limit to TBT passive Gen 2 cable
	 */
	if (is_tbt_compat_enabled(port)) {
		bool intel_svid = is_intel_svid(port, prev_svid_cnt);

		if (is_transmit_msg_sop_prime(port)) {
			if (!intel_svid)
				limit_tbt_cable_speed(port);
		} else if (intel_svid) {
			rsize = dfp_discover_svids(payload);
			enable_transmit_sop_prime(port);
			return 0;
		} else {
			disable_tbt_compat_mode(port);
		}
	}

	rsize = dfp_discover_modes(port, payload);

	disable_transmit_sop_prime(port);

	return rsize;
}

int dfp_handle_acked_discover_modes(int port, int cnt, uint32_t *payload)
{
	int rsize;

	dfp_consume_modes(port, cnt, payload);

	if (is_tbt_compat_enabled(port) &&
		is_tbt_compat_mode(port, cnt, payload))
		return process_tbt_compat_discover_modes(port, payload);

	rsize = dfp_discover_modes(port, payload);
	/* enter the default mode for DFP */
	if (!rsize) {
		/*
		 * Disabling Thunderbolt-Compatible mode if discover mode
		 * response doesn't include Intel SVID.
		 */
		disable_tbt_compat_mode(port);
		payload[0] = pd_dfp_enter_mode(port, 0, 0);
		if (payload[0])
			rsize = 1;
	}

	return rsize;
}

int dfp_handle_acked_enter_mode(int port, int cnt, uint32_t *payload,
				struct svdm_amode_data *modep)
{
	int rsize = 0;

	if (is_tbt_compat_enabled(port)) {
		rsize = enter_mode_tbt_compat(port, payload);
	/* Continue with PD flow if Thunderbolt-compatible mode is disabled */
	} else if (!modep) {
		rsize = 0;
	} else {
		if (!modep->opos)
			pd_dfp_enter_mode(port, 0, 0);

		if (modep->opos) {
			rsize = modep->fx->status(port, payload);
			payload[0] |= PD_VDO_OPOS(modep->opos);
		}
	}

	return rsize;
}

int dfp_handle_nacked_discover_svid(int port, int cnt, uint32_t *payload)
{
	int rsize = 0;

	if (is_tbt_compat_enabled(port) &&
	    is_transmit_msg_sop_prime(port) &&
	    get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE) {
		limit_tbt_cable_speed(port);
		rsize = dfp_discover_modes(port, payload);
		disable_transmit_sop_prime(port);
	}

	return rsize;
}
