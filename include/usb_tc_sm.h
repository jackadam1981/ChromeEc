/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* USB Type-C module */

#ifndef __CROS_EC_USB_TC_H
#define __CROS_EC_USB_TC_H

enum typec_state_id {
	DISABLED,
	UNATTACHED_SNK,
	ATTACH_WAIT_SNK,
	ATTACHED_SNK,
#if !defined(CONFIG_USB_TYPEC_VPD)
	ERROR_RECOVERY,
	UNATTACHED_SRC,
	ATTACH_WAIT_SRC,
	ATTACHED_SRC,
#endif
#if !defined(CONFIG_USB_TYPEC_CTVPD) && !defined(CONFIG_USB_TYPEC_VPD)
	AUDIO_ACCESSORY,
	ORIENTED_DEBUG_ACCESSORY_SRC,
	UNORIENTED_DEBUG_ACCESSORY_SRC,
	DEBUG_ACCESSORY_SNK,
	TRY_SRC,
	TRY_WAIT_SNK,
	CTUNATTACHED_SNK,
	CTATTACHED_SNK,
#endif
#if defined(CONFIG_USB_TYPEC_CTVPD)
	CTTRY_SNK,
	CTATTACHED_UNSUPPORTED,
	CTATTACH_WAIT_UNSUPPORTED,
	CTUNATTACHED_UNSUPPORTED,
	CTUNATTACHED_VPD,
	CTATTACH_WAIT_VPD,
	CTATTACHED_VPD,
	CTDISABLED_VPD,
	TRY_SNK,
	TRY_WAIT_SRC,
#endif
	/* Number of states. Not an actual state. */
	TC_STATE_COUNT,
};

/**
 * Get the id of the current Type-C state
 *
 * @param port USB-C port number
 */
enum typec_state_id get_typec_state_id(int port);

/**
 * Get current data role
 *
 * @param port USB-C port number
 * @return 0 for ufp, 1 for dfp, 2 for disconnected
 */
int tc_get_data_role(int port);

/**
 * Get current power role
 *
 * @param port USB-C port number
 * @return 0 for sink, 1 for source or vpd
 */
int tc_get_power_role(int port);

/**
 * Set the power role
 * This function should be used to temporarily set the
 * power role before communicating with a cable plug.
 *
 * @param port USB-C port number
 * @param role power role
 */
void tc_set_power_role(int port, int role);

/**
 * Set loop timeout value
 *
 * @param port USB-C port number
 * @timeout time in ms
 */
void tc_set_timeout(int port, uint64_t timeout);

/**
 * Initiates a Power Role Swap from Attached.SRC to Attached.SNK. This function
 * has no effect if the current Type-C state is not Attached.SRC.
 *
 * @param port USB_C port number
 */
void tc_prs_src_snk_assert_rd(int port);

/**
 * Initiates a Power Role Swap from Attached.SNK to Attached.SRC. This function
 * has no effect if the current Type-C state is not Attached.SNK.
 *
 * @param port USB_C port number
 */
void tc_prs_snk_src_assert_rp(int port);

/**
 * Informs the Type-C State Machine that a Power Role Swap is complete.
 * This function is called from the Policy Engine.
 *
 * @param port USB_C port number
 */
void tc_pr_swap_complete(int port);

/**
 * Instructs the Attached.SNK to stop drawing power. This function is called
 * from the Policy Engine and only has effect if the current Type-C state
 * Attached.SNK.
 *
 * @param port USB_C port number
 */
void tc_power_off_snk(int port);

/**
 * Instructs the Attached.SRC to stop supplying power. The function has
 * no effect if the current Type-C state is not Attached.SRC.
 *
 * @param port USB_C port number
 */
void tc_src_power_off(int port);

/**
 * Instructs the Attached.SRC to start supplying power. The function has
 * no effect if the current Type-C state is not Attached.SRC.
 *
 * @param port USB_C port number
 */
int tc_src_power_on(int port);

/**
 * Tests if a VCONN Swap is possible.
 *
 * @param port USB_C port number
 * @return 1 if vconn swap is possible, else 0
 */
int tc_check_vconn_swap(int port);

#ifdef CONFIG_USBC_VCONN
/**
 * Checks if VCONN is being sourced.
 *
 * @param port USB_C port number
 * @return 1 if vconn is being sourced, 0 if it's not, and -1 if
 *         can't answer at this time. -1 is returned if the current
 *         Type-C state is not Attached.SRC or Attached.SNK.
 */
int tc_is_vconn_src(int port);

/**
 * Instructs the Attached.SRC or Attached.SNK to start sourcing VCONN.
 * This function is called from the Policy Engine and only has effect
 * if the current Type-C state Attached.SRC or Attached.SNK.
 *
 * @param port USB_C port number
 */
void pd_request_vconn_swap_on(int port);

/**
 * Instructs the Attached.SRC or Attached.SNK to stop sourcing VCONN.
 * This function is called from the Policy Engine and only has effect
 * if the current Type-C state Attached.SRC or Attached.SNK.
 *
 * @param port USB_C port number
 */
void pd_request_vconn_swap_off(int port);
#endif

/**
 * Policy Engine informs the Type-C state machine if the port partner
 * is dualrole power.
 *
 * @param port USB_C port number
 * @param en   1 if port partner is dualrole power, else 0
 */
void tc_partner_dr_power(int port, int en);

/**
 * Policy Engine informs the Type-C state machine if the port partner
 * has external power
 *
 * @param port USB_C port number
 * @param en   1 if port partner has external power, else 0
 */
void tc_partner_extpower(int port, int en);

/**
 * Policy Engine informs the Type-C state machine if the port partner
 * is USB comms.
 *
 * @param port USB_C port number
 * @param en   1 if port partner is USB comms, else 0
 */
void tc_partner_usb_comm(int port, int en);

/**
 * Policy Engine informs the Type-C state machine if the port partner
 * is dualrole data.
 *
 * @param port USB_C port number
 * @param en   1 if port partner is dualrole data, else 0
 */
void tc_partner_dr_data(int port, int en);

/**
 * Policy Engine informs the Type-C state machine if the port partner
 * had a previous pd connection 
 *
 * @param port USB_C port number
 * @param en   1 if port partner had a previous pd connection, else 0
 */
void tc_pd_connection(int port, int en);

/**
 * Attempt to activate VCONN
 *
 * @param port USB-C port number
 */
void tc_vconn_on(int port);

/**
 * Start error recovery
 *
 * @param port USB-C port number
 */
void tc_start_error_recovery(int port);

/**
 * Hard Reset the TypeC port
 *
 * @param port USB-C port number
 */
void tc_hard_reset(int port);

#ifdef CONFIG_USB_TYPEC_CTVPD
/**
 * Resets the charge-through support timer. This can be
 * called many times but the support timer will only
 * reset once, while in the Attached.SNK state.
 *
 * @param port USB-C port number
 */
void tc_reset_support_timer(int port);
#else
void tc_ctvpd_detected(int port);
#endif /* CONFIG_USB_TYPEC_CTVPD */
#endif /* __CROS_EC_USB_TC_H */

