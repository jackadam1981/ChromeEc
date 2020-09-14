/* Copyright 2020 The Chromium OS Authors. All rights reserved.
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
#include "assert.h"
#include "usb_pd.h"
#include "usb_dp_alt_mode.h"
#include "usb_pd_dpm.h"
#include "usb_pd_tcpm.h"
#include "timer.h"

/*
 * Enter/Exit  DP mode with active cable
 *
 *
 *                       DP_START                           |------------
 *                 retry_done = false                       |           |
 *                           |                              v           |
 *                           |<------------------|    Exit Mode SOP     |
 *                           | retry_done = true |          |           |
 *                           v                   |          | ACK/NAK   |
 *                    Enter Mode SOP'            |  --------|---------  |
 *                       ACK | NAK               |    Exit Mode SOP''   |
 *                    |------|------|            |          |           |
 *                    |             |            |          | ACK/NAK   |
 *                    v             |            |  --------|---------  |
 *             Enter Mode SOP''     |            |     Exit Mode SOP'   |
 *                    |             |            |          |           |
 *                ACK | NAK         |            |          | ACK/NAK   |
 *             |------|-----------> |            |  ------------------  |
 *             |                    |            | retry_done == true?  |
 *             v                    |            |          |           |
 *       Enter Mode SOP             |            |   No     |           |
 *             |                    |            |-----------           |
 *         ACK | NAK                |                       |Yes        |
 *     |-------|------------------> |                       v           |
 *     |                            |                   DP_INACTIVE     |
 *     v                            |              retry_done = false   |
 * Status SOP'/''/SOP ------------> |                                   |
 *     |                            |                                   |
 *     |                            |                                   |
 *     v                            |                                   |
 * Configure SOP'/''/SOP ---------> |                                   |
 *     |                            |                                   |
 *     |                            |                                   |
 *     v                            |                                   |
 *  DP_ACTIVE                       |                                   |
 * retry_done = true                |                                   |
 *     |                            |                                   |
 *     v                            v                                   |
 *     -----------------------------------------------------------------|
 *
 */

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#else
#define CPRINTF(format, args...)
#define CPRINTS(format, args...)
#endif
	
static bool retry_done;

static int dp_prints(const char *string, int port)
{
	return CPRINTS("C%d: DPp %s", port, string);
}


/* The state of the DP negotiation */
enum dp_states {
	DP_START = 0,
	DP_ENTER_SOP,
	DP_STATUS_SOP,
	DP_CONFIG_SOP,
	DP_ACTIVE,
	DP_EXIT_SOP,
	DP_INACTIVE,
	/* Active cable only */
	DP_ENTER_SOP_PRIME,
	DP_ENTER_SOP_PRIME_PRIME,
	DP_STATUS_SOP_PRIME,
	DP_STATUS_SOP_PRIME_PRIME,
	DP_CONFIG_SOP_PRIME,
	DP_CONFIG_SOP_PRIME_PRIME,
	DP_EXIT_SOP_PRIME,
	DP_EXIT_SOP_PRIME_PRIME,
	DP_STATE_COUNT
};
static enum dp_states dp_state[CONFIG_USB_PD_PORT_MAX_COUNT];

/*
 * Map of states to expected VDM commands in responses.
 * Default of 0 indicates no command expected.
 */
static const uint8_t state_vdm_cmd[DP_STATE_COUNT] = {
	[DP_ENTER_SOP] = CMD_ENTER_MODE,
	[DP_STATUS_SOP] = CMD_DP_STATUS,
	[DP_CONFIG_SOP] = CMD_DP_CONFIG,
	[DP_ACTIVE] = CMD_EXIT_MODE,
	[DP_EXIT_SOP] = CMD_EXIT_MODE,
	/* Active cable only */
	[DP_ENTER_SOP_PRIME] = CMD_ENTER_MODE,
	[DP_ENTER_SOP_PRIME_PRIME] = CMD_ENTER_MODE,
	[DP_STATUS_SOP_PRIME] = CMD_DP_STATUS,
	[DP_STATUS_SOP_PRIME_PRIME] = CMD_DP_STATUS,
	[DP_CONFIG_SOP_PRIME] = CMD_DP_CONFIG,
	[DP_CONFIG_SOP_PRIME_PRIME] = CMD_DP_CONFIG,
	[DP_EXIT_SOP_PRIME] = CMD_EXIT_MODE,
	[DP_EXIT_SOP_PRIME_PRIME] = CMD_EXIT_MODE,
};

bool dp_is_active(int port)
{
	return dp_state[port] == DP_ACTIVE;
}

void dp_init(int port)
{
	dp_state[port] = DP_START;
	retry_done = false;
}

void dp_teardown(int port)
{
	CPRINTS("C%d: DP teardown", port);
	dp_state[port] = DP_INACTIVE;
	retry_done = false;
}

static void dp_entry_failed(int port)
{
	CPRINTS("C%d: DP alt mode protocol failed!", port);
	dp_state[port] = DP_INACTIVE;
	retry_done = false;
	dpm_set_mode_entry_done(port);
}

static bool dp_response_valid(int port, enum tcpm_transmit_type type,
			     char *cmdt, int vdm_cmd)
{
	enum dp_states st = dp_state[port];

	/*
	 * Check for an unexpected response.
	 * If DP is inactive, ignore the command.
	 */
	if ((st != DP_INACTIVE && state_vdm_cmd[st] != vdm_cmd) ||
	    (get_usb_pd_cable_type(port) == IDH_PTYPE_PCABLE &&
	     type != TCPC_TX_SOP)) {
		CPRINTS("C%d: Received unexpected DP VDM %s (cmd %d) from"
			" %s in state %d", port, cmdt, vdm_cmd,
			type == TCPC_TX_SOP ? "port partner" : "cable plug",
			st);
		dp_entry_failed(port);
		return false;
	}
	return true;
}

/* Exit Mode process is complete, but retry Enter Mode process */
static void dp_retry_enter_mode(int port)
{
	dp_state[port] = DP_START;
	retry_done = true;
}

/* Send Exit Mode to SOP''(if supported), or SOP' */
static void dp_active_cable_exit_mode(int port)
{
	struct pd_discovery *disc;

	disc = pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);

	if (disc->identity.product_t1.a_rev20.sop_p_p)
		dp_state[port] = DP_EXIT_SOP_PRIME_PRIME;
	else
		dp_state[port] = DP_EXIT_SOP_PRIME;
}


void dp_vdm_acked(int port, enum tcpm_transmit_type type, int vdo_count,
		uint32_t *vdm)
{
	/*
	const struct svdm_amode_data *modep =
		pd_get_amode_data(port, type, USB_SID_DISPLAYPORT);
	const uint8_t vdm_cmd = PD_VDO_CMD(vdm[0]);

	if (!dp_response_valid(port, type, "ACK", vdm_cmd))
		return;
	*/
	struct pd_discovery *disc;
	const uint8_t vdm_cmd = PD_VDO_CMD(vdm[0]);

	struct svdm_amode_data *modep;
	disc = pd_get_am_discovery(port, TCPC_TX_SOP_PRIME);
	
	if (!dp_response_valid(port, type, "ACK", vdm_cmd))
		return;

	/* TODO(b/155890173): Validate VDO count for specific commands */

	switch (dp_state[port]) {
	case DP_ENTER_SOP_PRIME:
		if (disc->identity.product_t1.a_rev20.sop_p_p)
			dp_state[port] = DP_ENTER_SOP_PRIME_PRIME;
		else
			dp_state[port] = DP_ENTER_SOP;
		dp_prints("enter mode SOP'", port);
		break;
	case DP_ENTER_SOP_PRIME_PRIME:
		dp_state[port] = DP_ENTER_SOP;
		dp_prints("enter mode SOP''", port);
		break;
	case DP_ENTER_SOP:
		//set_tbt_compat_mode_ready(port);
		//TODO: Make DP version of this

		//dpm_set_mode_entry_done(port);
		//TODO: No, not done yet with "Mode" Entry. Misnomer


		/* Active cable send Status SOP' first */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
			dp_state[port] = DP_STATUS_SOP_PRIME;
		} else {
			/* Passive cable send Status SOP */
			dp_state[port] = DP_STATUS_SOP;
		}
		//retry_done = true;
		//TODO: No, not done yet
		dp_prints("enter mode SOP", port);
		break;
	case DP_STATUS_SOP_PRIME:
		if (disc->identity.product_t1.a_rev20.sop_p_p)
			dp_state[port] = DP_STATUS_SOP_PRIME_PRIME;
		else
			dp_state[port] = DP_STATUS_SOP;
		dp_prints("status SOP'", port);
		break;
	case DP_STATUS_SOP_PRIME_PRIME:
		dp_state[port] = DP_STATUS_SOP;
		dp_prints("status SOP'", port);
		break;
	case DP_STATUS_SOP:
		/* DP status response & UFP's DP attention have same payload. */

		CPRINTS("C%d: caching STATUS with lvl %d irq %d", port,
			PD_VDO_DPSTS_HPD_LVL(vdm[1]), PD_VDO_DPSTS_HPD_IRQ(vdm[1]) );
		dfp_consume_attention(port, vdm);
		//dp_state[port] = DP_CONFIG_SOP;

		/* Active cable send Config SOP' first */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
			dp_state[port] = DP_CONFIG_SOP_PRIME;
		} else {
			/* Passive cable send Config SOP */
			dp_state[port] = DP_CONFIG_SOP;
		}
		dp_prints("status SOP", port);
		break;
	case DP_CONFIG_SOP_PRIME:
		if (disc->identity.product_t1.a_rev20.sop_p_p)
			dp_state[port] = DP_CONFIG_SOP_PRIME_PRIME;
		else
			dp_state[port] = DP_CONFIG_SOP;
		dp_prints("config SOP'", port);
		break;
	case DP_CONFIG_SOP_PRIME_PRIME:
		dp_state[port] = DP_CONFIG_SOP;
		dp_prints("config SOP'", port);
		break;
	case DP_CONFIG_SOP:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP, USB_SID_DISPLAYPORT);

		// CPRINTS("C%d: Adding 300ms delay post-config",port);
		// usleep(300000);
		// Not here either... needs to be post-mux

		if (modep && modep->opos && modep->fx->post_config)
			modep->fx->post_config(port);
		dpm_set_mode_entry_done(port);

		dp_state[port] = DP_ACTIVE;
		retry_done = true;
		//TODO: Yes, we're done.
		CPRINTS("C%d: Entered DP mode", port);
		break;
	case DP_ACTIVE:
		/*
		 * Request to exit mode successful, so put it in
		 * inactive state.
		 */
		CPRINTS("C%d: Exited DP mode", port);
		dp_state[port] = DP_INACTIVE;
		break;
	case DP_EXIT_SOP:
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE)
			dp_active_cable_exit_mode(port);
		else {
			if (retry_done)
				/* retried enter mode, still failed, give up */
				dp_entry_failed(port);
			else
				dp_retry_enter_mode(port);
		}
		break;
	case DP_EXIT_SOP_PRIME_PRIME:
		dp_state[port] = DP_EXIT_SOP_PRIME;
		break;
	case DP_EXIT_SOP_PRIME:
		if (retry_done) {
			/*
			 * Exit mode process is complete; go to inactive state.
			 */
			dp_prints("exit mode SOP'", port);
			dp_entry_failed(port);
		} else {
			dp_retry_enter_mode(port);
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
		CPRINTF("%s called with invalid state %d\n",
				__func__, dp_state[port]);
		dp_entry_failed(port);
		break;
	}
}

void dp_vdm_naked(int port, enum tcpm_transmit_type type, uint8_t vdm_cmd)
{
	if (!dp_response_valid(port, type, "NAK", vdm_cmd))
		return;

	switch (dp_state[port]) {
	case DP_ENTER_SOP_PRIME:
	case DP_ENTER_SOP_PRIME_PRIME:
	case DP_ENTER_SOP:
		/*
		 * If a request to enter DP mode is NAK'ed, this likely
		 * means the partner is already in DP alt mode, so
		 * request to exit the mode first before retrying
		 * the enter command. This can happen if the EC
		 * is restarted (e.g to go into recovery mode) while
		 * DP alt mode is active.
		 */
		dp_state[port] = DP_EXIT_SOP;
		break;
	case DP_ACTIVE:
		/* Exit SOP got NAK'ed */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE)
			dp_active_cable_exit_mode(port);
		else {
			dp_prints("exit mode SOP failed", port);
			dp_state[port] = DP_INACTIVE;
			retry_done = false;
		}
		break;
	case DP_EXIT_SOP:
		/* Exit SOP got NAK'ed */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE)
			dp_active_cable_exit_mode(port);
		else {
			if (retry_done)
				/* Retried enter mode, still failed, give up */
				dp_entry_failed(port);
			else
				dp_retry_enter_mode(port);
		}
		break;
	case DP_EXIT_SOP_PRIME_PRIME:
		dp_prints("exit mode SOP'' failed", port);
		dp_state[port] = DP_EXIT_SOP_PRIME;
		break;
	case DP_EXIT_SOP_PRIME:
		if (retry_done) {
			/*
			 * Exit mode process is complete; go to inactive state.
			 */
			dp_prints("exit mode SOP' failed", port);
			dp_entry_failed(port);
		} else {
			dp_retry_enter_mode(port);
		}
		break;
	default:
		CPRINTS("C%d: NAK for cmd %d in state %d", port,
			vdm_cmd, dp_state[port]);
		dp_entry_failed(port);
		break;
	}
}


	//HACKHACKHACK: TODO: DP AltMode v2.0 Spec: 5.3 Active Cable Support
	// Check (DP:Status(plug)) || (DP:Status(receptacle) && Cable(SSUSB))
	// DiscSVID Partner
	//  || UFP_U does not support the DisplayPort SVID
	// DiscMode Partner
	//	|| UFP_U-reported DisplayPort capabilities do not match the
	//		DFP_U-supported capabilities
	// DiscID Active Cable			
	//	|| Active cable does not respond –or– responds with a NAK
	// DiscSVID
	//  || Active cable responds, but does not support the DisplayPort SVID
	//  || Active cable-reported DP capabilities do not match
	//		DFP_U-supported capabilities


static bool dp_mode_is_supported(int port, int vdo_count,
	enum tcpm_transmit_type *tx_type)
{
	const struct pd_discovery *disc =
			pd_get_am_discovery(port, *tx_type);

	if (disc->identity.idh.modal_support && pd_is_mode_discovered_for_svid(port,
			*tx_type, USB_SID_DISPLAYPORT)) {
		return true;
	}	
	return false;
	/*
	return disc->identity.idh.modal_support &&
		is_tbt_cable_superspeed(port) &&
		get_tbt_cable_speed(port) >= TBT_SS_U31_GEN1;
	*/
}

static bool dp_mode_is_modal(int port, int vdo_count,
	enum tcpm_transmit_type *tx_type)
{
	const struct pd_discovery *disc =
			pd_get_am_discovery(port, *tx_type);

	if (disc->identity.idh.modal_support) {
		return true;
	}	
	return false;
}


/*
5 	DFP_U shall transmit a Discover Identity Command request addressed to SOP.
	a	No response is received, response is a NAK, –or– response is an ACK that indicates
		modal operation is not supported – DFP_U shall terminate the DisplayPort Alt Mode
		discovery and entry process.
	b	ACK response received that indicates modal operation is supported – DFP_U shall
		continue to step 6.

6 	If a cable response to the Discover Identity Command request addressed to SOP' did not
	occur in step 3, the request shall be repeated and the actions below shall be taken.
	a 	No response – DFP_U shall continue as described in Section 5.4.
	b 	USB PD Discover Identity VDO Product Type reports as a passive cable –
		DFP_U shall continue as described in Section 5.4.
	c 	USB PD Discover Identity VDO Product Type reports as an active cable –
		DFP_U shall continue to step 7.

7	Discover SVIDs Command request addressed to SOP.
	a 	UFP_U does not support the DisplayPort SVID – Terminate the DisplayPort Alt Mode
		discovery and entry process (this is no change from the passive mode cable case).

8	Discover Modes Command request addressed to SOP.
	a 	UFP_U-reported DisplayPort capabilities do not match the DFP_U-supported
		capabilities – Terminate the DisplayPort Alt Mode discovery and entry process
		(this is no change from the passive mode cable case).

9	Discover SVIDs Command request addressed to SOP'.
	a 	Active cable does not respond –or– responds with a NAK – Terminate the DisplayPort
		Alt Mode discovery and entry process.
	b 	Active cable responds, but does not support the DisplayPort SVID – Terminate the
		DisplayPort Alt Mode discovery and entry process.

10	Discover Modes Command request addressed to SOP'.
	a 	Active cable-reported DP capabilities do not match DFP_U-supported
		capabilities – Terminate the DisplayPort Alt Mode process.

11	Enter Mode Command request addressed to SOP'.

12	Optional – DisplayPort Status Update Command request addressed to SOP'.

13	Active cable VDO reports SOP'' as being supported – Enter Mode Command request
	addressed to SOP''. Active cable USB PD Discover Identity VDO is reported.

14 	Optional, active cable VDO reports SOP'' as being supported – DisplayPort Status Update
	Command request addressed to SOP''.

15 	Enter Mode Command request addressed to SOP.

16	DisplayPort Status Update VDO is exchanged between the DFP_U and UFP_U.

17	Place USB SuperSpeed in the Safe state, as appropriate.

18	DisplayPort Configure Command request addressed to SOP'.

19	Active cable VDO reports SOP'' as being supported – DisplayPort Configure Command
	request addressed to SOP''.

20 DisplayPort Configure Command request addressed to SOP.
*/


int dp_setup_next_vdm(int port, int vdo_count, uint32_t *vdm,
	enum tcpm_transmit_type *tx_type)
{
	struct svdm_amode_data *modep;
	int vdo_count_ret = 0;

	*tx_type = TCPC_TX_SOP;
	/*
	const struct svdm_amode_data *modep = pd_get_amode_data(port,
			TCPC_TX_SOP, USB_SID_DISPLAYPORT);
	int vdo_count_ret;
	*/

	if (vdo_count < VDO_MAX_SIZE)
		return -1;

	switch (dp_state[port]) {
	case DP_START:
		/* Enter the first supported mode for DisplayPort. */

		// HACKHACKHACK
		/*
		vdm[0] = pd_dfp_enter_mode(port, TCPC_TX_SOP_PRIME,
				USB_SID_DISPLAYPORT, 0);
		vdm[0] = pd_dfp_enter_mode(port, TCPC_TX_SOP_PRIME_PRIME,
				USB_SID_DISPLAYPORT, 0);
		*/

		// DP STEP 5
		if (!dp_mode_is_modal(port, vdo_count, TCPC_TX_SOP))
			return 0;

		// TODO: STEP 6 (Section 5.4) may branch differently

		// DP STEP 7
		if (!dp_mode_is_supported(port, vdo_count, TCPC_TX_SOP))
			return 0;

		if (!retry_done)
			dp_prints("attempt to enter mode", port);
		else
			dp_prints("retry to enter mode", port);

		/* Active cable send Enter Mode SOP' first */
		if (get_usb_pd_cable_type(port) == IDH_PTYPE_ACABLE) {
			vdo_count_ret =
				enter_dp_compat_mode(
					port, TCPC_TX_SOP_PRIME, vdm);
			*tx_type = TCPC_TX_SOP_PRIME;
			dp_state[port] = DP_ENTER_SOP_PRIME;
			dp_prints("Active",port);
		} else {
			/* Passive cable send Enter Mode SOP */
			vdo_count_ret =
				enter_dp_compat_mode(port, TCPC_TX_SOP, vdm);
			dp_state[port] = DP_ENTER_SOP;
			dp_prints("Passive",port);
		}

		CPRINTS("C%d: EXECUTE vdm 0x%08x count %d in state %d", port,
			vdm[0], vdo_count_ret, dp_state[port]);

//		vdm[0] = pd_dfp_enter_mode(port, TCPC_TX_SOP,
//				USB_SID_DISPLAYPORT, 0);
//		if (vdm[0] == 0)
//			return -1;
//		/* CMDT_INIT is 0, so this is a no-op */
//		vdm[0] |= VDO_CMDT(CMDT_INIT);
//		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP));
//		vdo_count_ret = 1;

		break;

	case DP_ENTER_SOP_PRIME:
		vdo_count_ret =
			enter_dp_compat_mode(port, TCPC_TX_SOP_PRIME, vdm);
		*tx_type = TCPC_TX_SOP_PRIME;
		dp_prints("attempt to enter mode SOP'", port);
		break;

	case DP_ENTER_SOP_PRIME_PRIME:
		vdo_count_ret =
			enter_dp_compat_mode(
				port, TCPC_TX_SOP_PRIME_PRIME, vdm);
		*tx_type = TCPC_TX_SOP_PRIME_PRIME;
		dp_prints("attempt to enter mode SOP''", port);
		break;
	case DP_ENTER_SOP:
		vdo_count_ret =
			enter_dp_compat_mode(port, TCPC_TX_SOP, vdm);
		dp_prints("attempt to enter mode SOP", port);
		break;
	case DP_STATUS_SOP_PRIME:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdo_count_ret = modep->fx->status(port, vdm);
		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= PD_VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		vdm[1] = 0x8;
		*tx_type = TCPC_TX_SOP_PRIME;
		dp_prints("attempt to status SOP'", port);
		break;
	case DP_STATUS_SOP_PRIME_PRIME:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdo_count_ret = modep->fx->status(port, vdm);
		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= PD_VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		vdm[1] = 0x8;
		*tx_type = TCPC_TX_SOP_PRIME_PRIME;
		dp_prints("attempt to status SOP''", port);
		break;
	case DP_STATUS_SOP:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdo_count_ret = modep->fx->status(port, vdm);
		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= PD_VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP));
		dp_prints("attempt to status SOP", port);
		break;
	case DP_CONFIG_SOP_PRIME:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		//vdo_count_ret = modep->fx->config(port, vdm);
		vdm[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_CONFIG | VDO_OPOS(modep->opos));
		vdm[1] = VDO_DP_CFG(get_dp_pin_mode(port),      /* pin mode */
				1,	       /* DPv1.3 signaling */
				2);	       /* UFP connected */
		vdo_count_ret=2;
		// HACKHACKHACK: MANUAL OVERRIDE

		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		*tx_type = TCPC_TX_SOP_PRIME;
		dp_prints("attempt to config SOP'", port);
		break;
	case DP_CONFIG_SOP_PRIME_PRIME:
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		//vdo_count_ret = modep->fx->config(port, vdm);
		vdm[0] = VDO(USB_SID_DISPLAYPORT, 1,
			 CMD_DP_CONFIG | VDO_OPOS(modep->opos));
		vdm[1] = VDO_DP_CFG(get_dp_pin_mode(port),      /* pin mode */
				1,	       /* DPv1.3 signaling */
				2);	       /* UFP connected */
		vdo_count_ret=2;
		// HACKHACKHACK: MANUAL OVERRIDE

		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		*tx_type = TCPC_TX_SOP_PRIME_PRIME;
		dp_prints("attempt to config SOP''", port);
		break;
	case DP_CONFIG_SOP:
		// dp_prints("Inserting CONFIG SOP delay'", port);
		// usleep(200000);
	//SBU hasn't been muxed yet... try another approach.
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdo_count_ret = modep->fx->config(port, vdm);  //<== = THIS! BUG!

/*
A DFP_U may transmit a DisplayPort Configure Command at any time while in DisplayPort Alt Mode.
Before issuing the command, the DFP_U shall place the USB-C pins that are to be reconfigured to
DisplayPort Configuration into the Safe state, as specified in USB-C (i.e., ensure that there is no
USB on these pins). The UFP_U may enable DisplayPort Alt Mode immediately after receiving
this command. The UFP_U shall respond to this command with a Responder ACK response after
*/

		if (vdo_count_ret == 0)
			return -1;
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP));
		dp_prints("attempt to config SOP", port);
		break;
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
		modep = pd_get_amode_data(port,
						TCPC_TX_SOP, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdm[0] = VDO(USB_SID_DISPLAYPORT,
			     1, /* structured */
			     CMD_EXIT_MODE);

		vdm[0] |= VDO_OPOS(modep->opos);
		vdm[0] |= VDO_CMDT(CMDT_INIT);
		vdm[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP));
		vdo_count_ret = 1;
		dp_prints("attempt to exit mode", port);
		break;
	case DP_EXIT_SOP_PRIME_PRIME:
		modep = pd_get_amode_data(port,
			TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdm[0] = VDO(USB_SID_DISPLAYPORT, 1, CMD_EXIT_MODE) |
			VDO_OPOS(modep->opos) |
			VDO_CMDT(CMDT_INIT) |
			VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		vdo_count_ret = 1;
		*tx_type = TCPC_TX_SOP_PRIME_PRIME;
		dp_prints("attempt to exit mode SOP''", port);
		break;
	case DP_EXIT_SOP_PRIME:
		modep = pd_get_amode_data(port,
				TCPC_TX_SOP_PRIME, USB_SID_DISPLAYPORT);
		if (!(modep && modep->opos))
			return -1;

		vdm[0] = VDO(USB_SID_DISPLAYPORT, 1, CMD_EXIT_MODE) |
			VDO_OPOS(modep->opos) |
			VDO_CMDT(CMDT_INIT) |
			VDO_SVDM_VERS(pd_get_vdo_ver(port, TCPC_TX_SOP_PRIME));
		vdo_count_ret = 1;
		*tx_type = TCPC_TX_SOP_PRIME;
		dp_prints("attempt to exit mode SOP'", port);
		break;
	case DP_INACTIVE:
		/*
		 * DP mode is inactive.
		 */
		return -1;
	default:
		CPRINTF("%s called with invalid state %d\n",
				__func__, dp_state[port]);
		return -1;
	}
	return vdo_count_ret;
}
