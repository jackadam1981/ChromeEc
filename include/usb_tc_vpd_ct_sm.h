/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "vpd_api.h"

/* USB Type-C CTVPD module */

#ifndef __CROS_EC_USB_TC_VPD_H
#define __CROS_EC_USB_TC_VPD_H

#undef PD_DEFAULT_STATE
/* Port default state at startup */
#define PD_DEFAULT_STATE(port) tc_state_unattached_snk

#define TC_OBJ(port)   (SM_OBJ(tc[port]))

static struct type_c {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* enable power delivery state machines */
	uint8_t pd_enable;
	/* event timeout */
	uint64_t evt_timeout;
	/* state machine event */
	int evt;
	/* Time a port shall wait before it can determine it is attached */
	uint64_t cc_debounce;
	/* Time a Sink port shall wait before it can determine it is detached
	 * due to the potential for USB PD signaling on CC as described in
	 * the state definitions.
	 */
	uint64_t pd_debounce;
	/* Maintains state of billboard device */
	int billboard_presented;
	/*
	 * Time a port shall wait before it can determine it is
	 * re-attached during the try-wait process.
	 */
	uint64_t try_wait_debounce;
	/* charge-through support timer */
	uint64_t support_timer;
	/* reset the charge-through support timer */
	int support_timer_reset;
	/* VPD host port cc state */
	enum pd_cc_states host_cc_state;
	enum pd_cc_states host_new_cc_state;
	uint8_t ct_cc;
	/* The cc state */
	enum pd_cc_states cc_state;
	enum pd_cc_states new_cc_state;
	uint64_t next_role_swap;
} tc[CONFIG_USB_PD_PORT_COUNT];

/* Type-C states */
static unsigned int tc_state_disabled(int port, enum signal sig);
static unsigned int tc_state_disabled_entry(int port);
static unsigned int tc_state_disabled_run(int port);
static unsigned int tc_state_disabled_exit(int port);

static unsigned int tc_state_error_recovery(int port, enum signal sig);
static unsigned int tc_state_error_recovery_entry(int port);
static unsigned int tc_state_error_recovery_run(int port);
static unsigned int tc_state_error_recovery_exit(int port);

static unsigned int tc_state_unattached_snk(int port, enum signal sig);
static unsigned int tc_state_unattached_snk_entry(int port);
static unsigned int tc_state_unattached_snk_run(int port);
static unsigned int tc_state_unattached_snk_exit(int port);

static unsigned int tc_state_attach_wait_snk(int port, enum signal sig);
static unsigned int tc_state_attach_wait_snk_entry(int port);
static unsigned int tc_state_attach_wait_snk_run(int port);
static unsigned int tc_state_attach_wait_snk_exit(int port);

static unsigned int tc_state_attached_snk(int port, enum signal sig);
static unsigned int tc_state_attached_snk_entry(int port);
static unsigned int tc_state_attached_snk_run(int port);
static unsigned int tc_state_attached_snk_exit(int port);

static unsigned int tc_state_try_snk(int port, enum signal sig);
static unsigned int tc_state_try_snk_entry(int port);
static unsigned int tc_state_try_snk_run(int port);
static unsigned int tc_state_try_snk_exit(int port);

static unsigned int tc_state_unattached_src(int port, enum signal sig);
static unsigned int tc_state_unattached_src_entry(int port);
static unsigned int tc_state_unattached_src_run(int port);
static unsigned int tc_state_unattached_src_exit(int port);

static unsigned int tc_state_attach_wait_src(int port, enum signal sig);
static unsigned int tc_state_attach_wait_src_entry(int port);
static unsigned int tc_state_attach_wait_src_run(int port);
static unsigned int tc_state_attach_wait_src_exit(int port);

static unsigned int tc_state_try_wait_src(int port, enum signal sig);
static unsigned int tc_state_try_wait_src_entry(int port);
static unsigned int tc_state_try_wait_src_run(int port);
static unsigned int tc_state_try_wait_src_exit(int port);

static unsigned int tc_state_attached_src(int port, enum signal sig);
static unsigned int tc_state_attached_src_entry(int port);
static unsigned int tc_state_attached_src_run(int port);
static unsigned int tc_state_attached_src_exit(int port);

static unsigned int tc_state_ct_try_snk(int port, enum signal sig);
static unsigned int tc_state_ct_try_snk_entry(int port);
static unsigned int tc_state_ct_try_snk_run(int port);
static unsigned int tc_state_ct_try_snk_exit(int port);

static unsigned int
	tc_state_ct_attach_wait_unsupported(int port, enum signal sig);
static unsigned int tc_state_ct_attach_wait_unsupported_entry(int port);
static unsigned int tc_state_ct_attach_wait_unsupported_run(int port);
static unsigned int tc_state_ct_attach_wait_unsupported_exit(int port);

static unsigned int tc_state_ct_attached_unsupported(int port, enum signal sig);
static unsigned int tc_state_ct_attached_unsupported_entry(int port);
static unsigned int tc_state_ct_attached_unsupported_run(int port);
static unsigned int tc_state_ct_attached_unsupported_exit(int port);

static unsigned int
	tc_state_ct_unattached_unsupported(int port, enum signal sig);
static unsigned int tc_state_ct_unattached_unsupported_entry(int port);
static unsigned int tc_state_ct_unattached_unsupported_run(int port);
static unsigned int tc_state_ct_unattached_unsupported_exit(int port);

static unsigned int tc_state_ct_unattached_vpd(int port, enum signal sig);
static unsigned int tc_state_ct_unattached_vpd_entry(int port);
static unsigned int tc_state_ct_unattached_vpd_run(int port);
static unsigned int tc_state_ct_unattached_vpd_exit(int port);

static unsigned int tc_state_ct_disabled_vpd(int port, enum signal sig);
static unsigned int tc_state_ct_disabled_vpd_entry(int port);
static unsigned int tc_state_ct_disabled_vpd_run(int port);
static unsigned int tc_state_ct_disabled_vpd_exit(int port);

static unsigned int tc_state_ct_attached_vpd(int port, enum signal sig);
static unsigned int tc_state_ct_attached_vpd_entry(int port);
static unsigned int tc_state_ct_attached_vpd_run(int port);
static unsigned int tc_state_ct_attached_vpd_exit(int port);

static unsigned int tc_state_ct_attach_wait_vpd(int port, enum signal sig);
static unsigned int tc_state_ct_attach_wait_vpd_entry(int port);
static unsigned int tc_state_ct_attach_wait_vpd_run(int port);
static unsigned int tc_state_ct_attach_wait_vpd_exit(int port);


/* Super States */
static unsigned int tc_state_host_rd_ct_rd(int port, enum signal sig);
static unsigned int tc_state_host_rd_ct_rd_entry(int port);
static unsigned int tc_state_host_rd_ct_rd_run(int port);
static unsigned int tc_state_host_rd_ct_rd_exit(int port);

static unsigned int tc_state_host_open_ct_open(int port, enum signal sig);
static unsigned int tc_state_host_open_ct_open_entry(int port);
static unsigned int tc_state_host_open_ct_open_run(int port);
static unsigned int tc_state_host_open_ct_open_exit(int port);

static unsigned int tc_state_vbus_cc_iso(int port, enum signal sig);
static unsigned int tc_state_vbus_cc_iso_entry(int port);
static unsigned int tc_state_vbus_cc_iso_run(int port);
static unsigned int tc_state_vbus_cc_iso_exit(int port);

static unsigned int tc_state_host_rp3_ct_rd(int port, enum signal sig);
static unsigned int tc_state_host_rp3_ct_rd_entry(int port);
static unsigned int tc_state_host_rp3_ct_rd_run(int port);
static unsigned int tc_state_host_rp3_ct_rd_exit(int port);

static unsigned int tc_state_host_rp3_ct_rpu(int port, enum signal sig);
static unsigned int tc_state_host_rp3_ct_rpu_entry(int port);
static unsigned int tc_state_host_rp3_ct_rpu_run(int port);
static unsigned int tc_state_host_rp3_ct_rpu_exit(int port);

static unsigned int tc_state_host_rpu_ct_rd(int port, enum signal sig);
static unsigned int tc_state_host_rpu_ct_rd_entry(int port);
static unsigned int tc_state_host_rpu_ct_rd_run(int port);
static unsigned int tc_state_host_rpu_ct_rd_exit(int port);

static unsigned int get_super_state(int port);


static const state_sig tc_state_disabled_sig[] = {
	tc_state_disabled_entry,
	tc_state_disabled_run,
	tc_state_disabled_exit,
	get_super_state
};

static const state_sig tc_state_error_recovery_sig[] = {
	tc_state_error_recovery_entry,
	tc_state_error_recovery_run,
	tc_state_error_recovery_exit,
	get_super_state
};

static const state_sig tc_state_unattached_snk_sig[] = {
	tc_state_unattached_snk_entry,
	tc_state_unattached_snk_run,
	tc_state_unattached_snk_exit,
	get_super_state
};

static const state_sig tc_state_attach_wait_snk_sig[] = {
	tc_state_attach_wait_snk_entry,
	tc_state_attach_wait_snk_run,
	tc_state_attach_wait_snk_exit,
	get_super_state
};

static const state_sig tc_state_attached_snk_sig[] = {
	tc_state_attached_snk_entry,
	tc_state_attached_snk_run,
	tc_state_attached_snk_exit,
	get_super_state
};

static const state_sig tc_state_try_snk_sig[] = {
	tc_state_try_snk_entry,
	tc_state_try_snk_run,
	tc_state_try_snk_exit,
	get_super_state
};

static const state_sig tc_state_unattached_src_sig[] = {
	tc_state_unattached_src_entry,
	tc_state_unattached_src_run,
	tc_state_unattached_src_exit,
	get_super_state
};

static const state_sig tc_state_attach_wait_src_sig[] = {
	tc_state_attach_wait_src_entry,
	tc_state_attach_wait_src_run,
	tc_state_attach_wait_src_exit,
	get_super_state
};

static const state_sig tc_state_try_wait_src_sig[] = {
	tc_state_try_wait_src_entry,
	tc_state_try_wait_src_run,
	tc_state_try_wait_src_exit,
	get_super_state
};

static const state_sig tc_state_attached_src_sig[] = {
	tc_state_attached_src_entry,
	tc_state_attached_src_run,
	tc_state_attached_src_exit,
	get_super_state
};

static const state_sig tc_state_ct_try_snk_sig[] = {
	tc_state_ct_try_snk_entry,
	tc_state_ct_try_snk_run,
	tc_state_ct_try_snk_exit,
	get_super_state
};

static const state_sig tc_state_ct_attach_wait_unsupported_sig[] = {
	tc_state_ct_attach_wait_unsupported_entry,
	tc_state_ct_attach_wait_unsupported_run,
	tc_state_ct_attach_wait_unsupported_exit,
	get_super_state
};

static const state_sig tc_state_ct_attached_unsupported_sig[] = {
	tc_state_ct_attached_unsupported_entry,
	tc_state_ct_attached_unsupported_run,
	tc_state_ct_attached_unsupported_exit,
	get_super_state
};

static const state_sig tc_state_ct_unattached_unsupported_sig[] = {
	tc_state_ct_unattached_unsupported_entry,
	tc_state_ct_unattached_unsupported_run,
	tc_state_ct_unattached_unsupported_exit,
	get_super_state
};

static const state_sig tc_state_ct_unattached_vpd_sig[] = {
	tc_state_ct_unattached_vpd_entry,
	tc_state_ct_unattached_vpd_run,
	tc_state_ct_unattached_vpd_exit,
	get_super_state
};

static const state_sig tc_state_ct_disabled_vpd_sig[] = {
	tc_state_ct_disabled_vpd_entry,
	tc_state_ct_disabled_vpd_run,
	tc_state_ct_disabled_vpd_exit,
	get_super_state
};

static const state_sig tc_state_ct_attached_vpd_sig[] = {
	tc_state_ct_attached_vpd_entry,
	tc_state_ct_attached_vpd_run,
	tc_state_ct_attached_vpd_exit,
	get_super_state
};

static const state_sig tc_state_ct_attach_wait_vpd_sig[] = {
	tc_state_ct_attach_wait_vpd_entry,
	tc_state_ct_attach_wait_vpd_run,
	tc_state_ct_attach_wait_vpd_exit,
	get_super_state
};

static const state_sig tc_state_host_rd_ct_rd_sig[] = {
	tc_state_host_rd_ct_rd_entry,
	tc_state_host_rd_ct_rd_run,
	tc_state_host_rd_ct_rd_exit,
	get_super_state
};

static const state_sig tc_state_host_open_ct_open_sig[] = {
	tc_state_host_open_ct_open_entry,
	tc_state_host_open_ct_open_run,
	tc_state_host_open_ct_open_exit,
	get_super_state
};

static const state_sig tc_state_vbus_cc_iso_sig[] = {
	tc_state_vbus_cc_iso_entry,
	tc_state_vbus_cc_iso_run,
	tc_state_vbus_cc_iso_exit,
	get_super_state
};

static const state_sig tc_state_host_rp3_ct_rd_sig[] = {
	tc_state_host_rp3_ct_rd_entry,
	tc_state_host_rp3_ct_rd_run,
	tc_state_host_rp3_ct_rd_exit,
	get_super_state
};

static const state_sig tc_state_host_rp3_ct_rpu_sig[] = {
	tc_state_host_rp3_ct_rpu_entry,
	tc_state_host_rp3_ct_rpu_run,
	tc_state_host_rp3_ct_rpu_exit,
	get_super_state
};

static const state_sig tc_state_host_rpu_ct_rd_sig[] = {
	tc_state_host_rpu_ct_rd_entry,
	tc_state_host_rpu_ct_rd_run,
	tc_state_host_rpu_ct_rd_exit,
	get_super_state
};


void tc_reset_support_timer(int port)
{
	tc[port].support_timer_reset++;
}

static void tc_state_init(int port)
{
	int res = 0;
	sm_state this_state;

	res = pd_restart_tcpc(port);

	CPRINTS("TCPC p%d init %s", port, res ? "failed" : "ready");
	this_state = res ? tc_state_disabled : PD_DEFAULT_STATE(port);

	/* Disable TCPC RX until connection is established */
	tcpm_set_rx_enable(port, 0);

	init_state(port, TC_OBJ(port), this_state);

	/* Disable pd state machines */
	tc[port].pd_enable = 0;
	tc[port].evt_timeout = 10*MSEC;
	tc[port].power_role = PD_ROLE_VPD;
	tc[port].data_role = 0; /* Reserved for VPD */
	tc[port].billboard_presented = 0;
}

/*
 * Disabled
 */
static unsigned int tc_state_disabled(int port, enum signal sig)
{
	int ret = 0;

	ret = (*tc_state_disabled_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_open_ct_open);
}

static unsigned int tc_state_disabled_entry(int port)
{
	CPRINTS("C%d: Disabled", port);
	return 0;
}

static unsigned int tc_state_disabled_run(int port)
{
	task_wait_event(-1);
	return RUN_SUPER;
}

static unsigned int tc_state_disabled_exit(int port)
{
#ifndef CONFIG_USB_PD_TCPC
	if (pd_restart_tcpc(port) != 0) {
		CPRINTS("TCPC p%d restart failed!", port);
		return 0;
	}
#endif
	CPRINTS("TCPC p%d resumed!", port);
	set_state(port, TC_OBJ(port), tc_state_unattached_snk);

	return 0;
}

/*
 * ErrorRecovery
 */
static unsigned int tc_state_error_recovery(int port, enum signal sig)
{
	int ret = 0;

	ret = (*tc_state_error_recovery_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_open_ct_open);
}

static unsigned int tc_state_error_recovery_entry(int port)
{
	/* Use cc_debounce state variable for error recovery timeout */
	tc[port].cc_debounce = get_time().val + PD_T_ERROR_RECOVERY;
	return 0;
}

static unsigned int tc_state_error_recovery_run(int port)
{
	if (get_time().val > tc[port].cc_debounce) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_error_recovery_exit(int port)
{
	return 0;
}

/*
 * Unattached.SNK
 */
static unsigned int tc_state_unattached_snk(int port, enum signal sig)
{
	int ret = 0;

	ret = (*tc_state_unattached_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rd_ct_rd);
}

static unsigned int tc_state_unattached_snk_entry(int port)
{
	if (tc[port].obj.last_state != tc_state_unattached_src)
		CPRINTS("C%d: Unattached.SNK", port);

	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;
	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_unattached_snk_run(int port)
{
	int host_cc;
	int cc1;
	int cc2;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	/* Check host CC for connection */
	if (cc_is_rp(host_cc))
		tc[port].host_new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	/*
	 * Transition to AttachWait.SNK when a Source connection is
	 * detected, as indicated by the SNK.Rp state on its Host-side
	 * port’s CC pin.
	 */
	if (tc[port].host_new_cc_state == PD_CC_DFP_ATTACHED) {
		set_state(port, TC_OBJ(port), tc_state_attach_wait_snk);
		return 0;
	}

	/* Debounce Host CC state */
	if (tc[port].host_new_cc_state != tc[port].host_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].next_role_swap = get_time().val + PD_T_DRP_SRC;
		return 0;
	}

	/* Wait for Host CC debounce */
	if (get_time().val < tc[port].next_role_swap)
		return 0;

	/*
	 * If we are here, Host cc is open and debounced
	 */

	/* Check Charge-Through CCs for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	/*
	 * Transition to Unattached.SRC when SNK.Rp state is detected on
	 * exactly one of the the Charge-Through CC1 or CC2 pins for
	 * at least tCCDebounce and VBUS is detected
	 */
	if ((cc_is_rp(cc1) && !cc_is_rp(cc2)) ||
					(!cc_is_rp(cc1) && cc_is_rp(cc2)))
		tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].new_cc_state = PD_CC_NONE;

	/* Debounce Charge-Through CC state */
	if (tc[port].cc_state != tc[port].new_cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		return 0;
	}

	/* Wait for CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	if (vpd_is_ct_vbus() && tc[port].cc_state == PD_CC_DFP_ATTACHED) {
		set_state(port, TC_OBJ(port), tc_state_unattached_src);
		return 0;
	}

	return RUN_SUPER;
}


static unsigned int tc_state_unattached_snk_exit(int port)
{
	return 0;
}

/*
 * AttachWait.SNK
 */
static unsigned int tc_state_attach_wait_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attach_wait_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rd_ct_rd);
}

static unsigned int tc_state_attach_wait_snk_entry(int port)
{
	CPRINTS("C%d: AttachWait.SNK", port);
	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_attach_wait_snk_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	if (cc_is_rp(host_cc))
		tc[port].host_new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	/* Debounce the Host CC state */
	if (tc[port].host_cc_state != tc[port].host_new_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		return 0;
	}

	/* Wait for Host CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	if (tc[port].host_cc_state == PD_CC_DFP_ATTACHED &&
			(vpd_is_vconn() || vpd_is_host_vbus()))
		set_state(port, TC_OBJ(port), tc_state_attached_snk);
	else
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);

	return 0;
}

static unsigned int tc_state_attach_wait_snk_exit(int port)
{
	return 0;
}

/*
 * Attached.SNK
 */
static unsigned int tc_state_attached_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attached_snk_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_attached_snk_entry(int port)
{
	CPRINTS("C%d: Attached.SNK", port);

	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	/*
	 * This state can only be entered from states AttachWait.SNK
	 * and Try.SNK. So the Host port is isolated from the
	 * Charge-Through port. We only need to High-Z the
	 * Charge-Through ports CC1 and CC2 pins.
	 */
	vpd_ct_set_pull(TYPEC_CC_OPEN, 0);

	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;

	/* Start Charge-Through support timer */
	tc[port].support_timer_reset = 0;
	tc[port].support_timer = get_time().val + PD_T_AME;

	return 0;
}

static unsigned int tc_state_attached_snk_run(int port)
{
	int host_cc;

	/*
	 * Reset the Charge-Through Support Timer when it first
	 * receives any USB PD Structured VDM Command it supports,
	 * which is the Discover Identity command. And this is only
	 * done one time.
	 */
	if (tc[port].support_timer_reset == 1) {
		tc[port].support_timer_reset++;
		tc[port].support_timer = get_time().val + PD_T_AME;
	}

	/* Has host vbus and vconn been removed */
	if (!vpd_is_host_vbus() && !vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	if (cc_is_rp(host_cc))
		tc[port].host_new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	/* Debounce the Host CC state */
	if (tc[port].host_cc_state != tc[port].host_new_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_VPDCTDD;
		return 0;
	}

	/* Wait for Host CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	if (tc[port].host_cc_state == PD_CC_NONE && vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_vpd);
		return 0;
	}

	/* Check the Support Timer */
	if (get_time().val > tc[port].support_timer &&
					!tc[port].billboard_presented) {
		/*
		 * Present USB Billboard Device Class interface
		 * indicating that Charge-Through is not supported
		 */
		tc[port].billboard_presented = 1;
		vpd_present_billboard(BB_SNK);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_attached_snk_exit(int port)
{
	tc[port].billboard_presented = 0;
	vpd_present_billboard(BB_NONE);

	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * Super State Host_Rd_Ct_Rd
 */
static unsigned int tc_state_host_rd_ct_rd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_host_rd_ct_rd_sig[sig])(port);
	return SUPER(ret, sig, tc_state_vbus_cc_iso);
}

static unsigned int tc_state_host_rd_ct_rd_entry(int port)
{
	/* Place Rd on Host CC */
	vpd_host_set_pull(TYPEC_CC_RD, 0);

	/* Place Rd on Charge-Through CCs */
	vpd_ct_set_pull(TYPEC_CC_RD, 0);

	return 0;
}

static unsigned int tc_state_host_rd_ct_rd_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_host_rd_ct_rd_exit(int port)
{
	return 0;
}

/*
 * Super State HOST_OPEN_CT_OPEN
 */
static unsigned int tc_state_host_open_ct_open(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_host_open_ct_open_sig[sig])(port);
	return SUPER(ret, sig, tc_state_vbus_cc_iso);
}

static unsigned int tc_state_host_open_ct_open_entry(int port)
{
	/* Remove the terminations from Host */
	vpd_host_set_pull(TYPEC_CC_OPEN, 0);

	/* Remove the terminations from Charge-Through */
	vpd_ct_set_pull(TYPEC_CC_OPEN, 0);

	return 0;
}

static unsigned int tc_state_host_open_ct_open_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_host_open_ct_open_exit(int port)
{
	return 0;
}

/*
 * Super State Vbus_CC_Iso
 */
static unsigned int tc_state_vbus_cc_iso(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_vbus_cc_iso_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_vbus_cc_iso_entry(int port)
{
	/* Isolate the Host-side port from the Charge-Through port */
	vpd_vbus_pass_en(0);
	vpd_ct_cc_sel(CT_OPEN);

	/* Allow control of Host CC Rd with cc_RP3A0_RD_L */
	vpd_cc_db_en_od(GPO_LOW);

	/* Enable mcu communication and cc */
	vpd_mcu_cc_en(1);

	return 0;
}

static unsigned int tc_state_vbus_cc_iso_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_vbus_cc_iso_exit(int port)
{
	return 0;
}

/*
 * Unattached.SRC
 */
static unsigned int tc_state_unattached_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_unattached_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rpu_ct_rd);
}

static unsigned int tc_state_unattached_src_entry(int port)
{
	if (tc[port].obj.last_state != tc_state_unattached_snk)
		CPRINTS("C%d: Unattached.SRC", port);

	/* Get power from VBUS */
	vpd_vconn_pwr_sel_odl(PWR_VBUS);

	/* Make sure it's the Charge-Through Port's VBUS */
	if (!vpd_is_ct_vbus()) {
		set_state(port, TC_OBJ(port), tc_state_error_recovery);
		return 0;
	}

	tc[port].next_role_swap = get_time().val + PD_T_DRP_SRC;

	return 0;
}

static unsigned int tc_state_unattached_src_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	/*
	 * Transition to AttachWait.SRC when host-side VBUS is
	 * vSafe0V and SRC.Rd state is detected on the Host-side
	 * port’s CC pin.
	 */
	if (!vpd_is_host_vbus() && host_cc == TYPEC_CC_VOLT_RD) {
		set_state(port, TC_OBJ(port), tc_state_attach_wait_src);
		return 0;
	}

	/*
	 * Transition to Unattached.SNK within tDRPTransition or
	 * if Charge-Through VBUS is removed.
	 */
	if (!vpd_is_ct_vbus() || get_time().val > tc[port].next_role_swap) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_unattached_src_exit(int port)
{
	return 0;
}

/*
 * AttachWait.SRC
 */
static unsigned int tc_state_attach_wait_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attach_wait_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rpu_ct_rd);
}

static unsigned int tc_state_attach_wait_src_entry(int port)
{
	CPRINTS("C%d: AttachWait.SRC", port);

	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_attach_wait_src_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	if (host_cc == TYPEC_CC_VOLT_RD)
		tc[port].host_new_cc_state = PD_CC_UFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	if (tc[port].host_new_cc_state == PD_CC_NONE || !vpd_is_ct_vbus()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	/* Debounce the Host CC state */
	if (tc[port].host_cc_state != tc[port].host_new_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].cc_debounce = get_time().val +	PD_T_CC_DEBOUNCE;
		return 0;
	}

	/* Wait for Host CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	/* Debounce complete */
	if (tc[port].host_cc_state == PD_CC_UFP_ATTACHED &&
							!vpd_is_host_vbus()) {
		set_state(port, TC_OBJ(port), tc_state_try_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_attach_wait_src_exit(int port)
{
	return 0;
}

/*
 * Attached.SRC
 */
static unsigned int tc_state_attached_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attached_src_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_attached_src_entry(int port)
{
	CPRINTS("C%d: Attached.SRC", port);

	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	/* Connect Charge-Through VBUS to Host VBUS */
	vpd_vbus_pass_en(1);

	/*
	 * Get power from VBUS. No need to test because
	 * the Host VBUS is connected to the Charge-Through
	 * VBUS
	 */
	vpd_vconn_pwr_sel_odl(PWR_VBUS);

	return 0;
}

static unsigned int tc_state_attached_src_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);
	if (!vpd_is_ct_vbus() || host_cc == TYPEC_CC_VOLT_OPEN) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_attached_src_exit(int port)
{
	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * Super State Host_Rpu_Ct_Rd
 */
static unsigned int tc_state_host_rpu_ct_rd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_host_rpu_ct_rd_sig[sig])(port);
	return SUPER(ret, sig, tc_state_vbus_cc_iso);
}

static unsigned int tc_state_host_rpu_ct_rd_entry(int port)
{
	/* Place RpUSB on Host CC */
	vpd_host_set_pull(TYPEC_CC_RP, TYPEC_RP_USB);

	/* Place Rd on Charge-Through CCs */
	vpd_ct_set_pull(TYPEC_CC_RD, 0);

	return 0;
}

static unsigned int tc_state_host_rpu_ct_rd_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_host_rpu_ct_rd_exit(int port)
{
	return 0;
}

/*
 * Try.SNK
 */
static unsigned int tc_state_try_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_try_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rd_ct_rd);
}

static unsigned int tc_state_try_snk_entry(int port)
{
	CPRINTS("C%d: Try.SNK", port);

	/* Get power from VBUS */
	vpd_vconn_pwr_sel_odl(PWR_VBUS);

	/* Make sure it's the Charge-Through Port's VBUS */
	if (!vpd_is_ct_vbus()) {
		set_state(port, TC_OBJ(port), tc_state_error_recovery);
		return 0;
	}

	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;

	/* Using next_role_swap timer as try_src timer */
	tc[port].next_role_swap = get_time().val + PD_T_TRY_SRC;

	return 0;
}

static unsigned int tc_state_try_snk_run(int port)
{
	int host_cc;

	/*
	 * Wait for tDRPTry before monitoring the Charge-Through
	 * port’s CC pins for the SNK.Rp
	 */
	if (get_time().val < tc[port].next_role_swap)
		return 0;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	if (cc_is_rp(host_cc))
		tc[port].host_new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	/* Debounce the Host CC state */
	if (tc[port].host_cc_state != tc[port].host_new_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_DEBOUNCE;
		return 0;
	}

	/* Wait for Host CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	/* Debounce complete */
	if (tc[port].host_cc_state == PD_CC_DFP_ATTACHED &&
			(vpd_is_host_vbus() || vpd_is_vconn()))
		set_state(port, TC_OBJ(port), tc_state_attached_snk);
	else
		set_state(port, TC_OBJ(port), tc_state_try_wait_src);

	return 0;
}

static unsigned int tc_state_try_snk_exit(int port)
{
	return 0;
}

/*
 * TryWait.SRC
 */
static unsigned int tc_state_try_wait_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_try_wait_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rpu_ct_rd);
}

static unsigned int tc_state_try_wait_src_entry(int port)
{
	CPRINTS("C%d: TryWait.SRC", port);

	tc[port].host_cc_state = -1;
	tc[port].host_new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_try_wait_src_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	if (host_cc == TYPEC_CC_VOLT_RD)
		tc[port].host_new_cc_state = PD_CC_UFP_ATTACHED;
	else
		tc[port].host_new_cc_state = PD_CC_NONE;

	/* Debounce the Host CC state */
	if (tc[port].host_cc_state != tc[port].host_new_cc_state) {
		tc[port].host_cc_state = tc[port].host_new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_DEBOUNCE;
		tc[port].next_role_swap = get_time().val + PD_T_TRY_SRC;
		return 0;
	}

	if (get_time().val > tc[port].cc_debounce) {
		/* Debounce complete */
		if (tc[port].host_cc_state == PD_CC_UFP_ATTACHED &&
							!vpd_is_host_vbus()) {
			set_state(port, TC_OBJ(port), tc_state_attached_src);
			return 0;
		}
	}

	if (get_time().val > tc[port].next_role_swap) {
		/* Debounce complete */
		if (tc[port].host_cc_state == PD_CC_NONE) {
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
			return 0;
		}
	}

	return RUN_SUPER;
}

static unsigned int tc_state_try_wait_src_exit(int port)
{
	return 0;
}

/*
 * CTTry.SNK
 */
static unsigned int tc_state_ct_try_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_try_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rd);
}

static unsigned int tc_state_ct_try_snk_entry(int port)
{
	CPRINTS("C%d: CTTry.SNK", port);

	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;
	tc[port].next_role_swap = get_time().val + PD_T_TRY_SRC;

	return 0;
}

static unsigned int tc_state_ct_try_snk_run(int port)
{
	int cc1;
	int cc2;

	/*
	 * Wait for tDRPTry before monitoring the Charge-Through
	 * port’s CC pins for the SNK.Rp
	 */
	if (get_time().val < tc[port].next_role_swap)
		return 0;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	if (cc_is_rp(cc1) || cc_is_rp(cc2))
		tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	else
		tc[port].new_cc_state = PD_CC_NONE;

	/* Debounce the CT CC state */
	if (tc[port].cc_state != tc[port].new_cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_DEBOUNCE;
		tc[port].try_wait_debounce = get_time().val + PD_T_TRY_WAIT;

		return 0;
	}

	if (get_time().val > tc[port].cc_debounce) {
		/* Debounce complete */
		if (tc[port].cc_state == PD_CC_DFP_ATTACHED &&
				vpd_is_ct_vbus()) {
			set_state(port, TC_OBJ(port), tc_state_ct_attached_vpd);
			return 0;
		}
	}

	if (get_time().val > tc[port].try_wait_debounce) {
		/* Debounce complete */
		if (tc[port].cc_state == PD_CC_NONE) {
			set_state(port, TC_OBJ(port),
					tc_state_ct_attached_unsupported);
			return 0;
		}
	}

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_try_snk_exit(int port)
{
	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * CTAttachWait.Unsupported
 */
static unsigned int
	tc_state_ct_attach_wait_unsupported(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_attach_wait_unsupported_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rpu);
}

static unsigned int tc_state_ct_attach_wait_unsupported_entry(int port)
{
	CPRINTS("C%d: CTAttachWait.Unsupported", port);

	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_ct_attach_wait_unsupported_run(int port)
{
	int cc1;
	int cc2;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	if (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_OPEN)
		tc[port].new_cc_state = PD_CC_NONE;
	else if (cc1 == TYPEC_CC_VOLT_RD || cc2 == TYPEC_CC_VOLT_RD)
		tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	else if (cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_RA)
		tc[port].new_cc_state = PD_CC_AUDIO_ACC;

	/* Debounce the cc state */
	if (tc[port].cc_state != tc[port].new_cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		return 0;
	}

	/* Wait for CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	/* Debounce complete */
	if (tc[port].new_cc_state == PD_CC_DFP_ATTACHED) {
		set_state(port, TC_OBJ(port), tc_state_ct_try_snk);
		return 0;
	}

	if (tc[port].new_cc_state == PD_CC_NONE) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_vpd);
		return 0;
	}

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_attach_wait_unsupported_exit(int port)
{
	return 0;
}

/*
 * CTAttached.Unsupported
 */
static unsigned int tc_state_ct_attached_unsupported(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_attached_unsupported_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rpu);
}

static unsigned int tc_state_ct_attached_unsupported_entry(int port)
{
	CPRINTS("C%d: CTAttached.Unsupported", port);

	/* Get Power from VCONN */
	vpd_vconn_pwr_sel_odl(PWR_VCONN);

	/* Present Billboard device */
	vpd_present_billboard(BB_SRC);

	return 0;
}

static unsigned int tc_state_ct_attached_unsupported_run(int port)
{
	int cc1;
	int cc2;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	if ((cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_OPEN) ||
	    (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_RA) ||
	    (cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_OPEN)) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_vpd);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_attached_unsupported_exit(int port)
{
	vpd_present_billboard(BB_NONE);

	return 0;
}

/*
 * CTUnattached.Unsupported
 */
static unsigned int
	tc_state_ct_unattached_unsupported(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_unattached_unsupported_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rpu);
}

static unsigned int tc_state_ct_unattached_unsupported_entry(int port)
{
	CPRINTS("C%d: CTUnattached.Unsupported", port);
	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	tc[port].next_role_swap = get_time().val + PD_T_DRP_SRC;

	return 0;
}

static unsigned int tc_state_ct_unattached_unsupported_run(int port)
{
	int cc1;
	int cc2;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	if ((cc1 == TYPEC_CC_VOLT_RD || cc2 == TYPEC_CC_VOLT_RD) ||
			(cc1 == TYPEC_CC_VOLT_RA &&
			cc2 == TYPEC_CC_VOLT_RA)) {
		set_state(port, TC_OBJ(port),
				tc_state_ct_attach_wait_unsupported);
		return 0;
	}

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	if (get_time().val > tc[port].next_role_swap) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_vpd);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_unattached_unsupported_exit(int port)
{
	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * CTUnattached.VPD
 */
static unsigned int tc_state_ct_unattached_vpd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_unattached_vpd_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rd);
}

static unsigned int tc_state_ct_unattached_vpd_entry(int port)
{
	CPRINTS("C%d: CTUnattached.VPD", port);

	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_ct_unattached_vpd_run(int port)
{
	int cc1;
	int cc2;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	/*
	 * Transition to Unattached.SRC SNK.Rp state is detected on
	 * exactly one of the the Charge-Through CC1 or CC2 pins for
	 * at least tCCDebounce and VBUS is detected
	 */
	if ((cc_is_rp(cc1) && !cc_is_rp(cc2)) ||
				(!cc_is_rp(cc1) && cc_is_rp(cc2)))
		tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	else if (!cc_is_rp(cc1) && !cc_is_rp(cc2))
		tc[port].new_cc_state = PD_CC_NONE;
	else
		tc[port].new_cc_state = -1;

	if (tc[port].new_cc_state == PD_CC_DFP_ATTACHED) {
		set_state(port, TC_OBJ(port),
					tc_state_ct_attach_wait_vpd);
		return 0;
	}

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port),
				tc_state_ct_unattached_unsupported);
		return 0;
	}

	/* Debounce the cc state */
	if (tc[port].new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_DRP_SRC;
		return 0;
	}

	if (get_time().val < tc[port].cc_debounce)
		return 0;

		/* Debounce complete */
	if (tc[port].cc_state == PD_CC_NONE) {
		set_state(port, TC_OBJ(port),
			tc_state_ct_unattached_unsupported);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_unattached_vpd_exit(int port)
{
	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * CTDisabled.VPD
 */
static unsigned int tc_state_ct_disabled_vpd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_disabled_vpd_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_ct_disabled_vpd_entry(int port)
{
	CPRINTS("C%d: CTDisable.VPD", port);

	/* Get power from VBUS */
	vpd_vconn_pwr_sel_odl(PWR_VBUS);

	/*
	 * Set CC1_RPUSB_ODH and CC2_RPUSB_ODH to Hi-Z for
	 * Charge-Through CC1 and CC2.
	 */
	vpd_config_cc1_rpusb_odh(PIN_ADC, 0);
	vpd_config_cc2_rpusb_odh(PIN_ADC, 0);

	/*
	 * Set CC1_RP1A5_odh AND CC2_RP1A5_ODH to Hi-Z for
	 * Charge-Through CC1 and CC2.
	 */
	vpd_config_cc1_rp1a5_odh(PIN_ADC, 0);
	vpd_config_cc2_rp1a5_odh(PIN_ADC, 0);

	/* Set CC_VPDMCU as Hi-Z for Host CC */
	vpd_config_cc_vpdmcu(PIN_ADC, 0);

	/* Isolate the Host-side port from the Charge-Through port */
	vpd_vbus_pass_en(0);
	vpd_ct_cc_sel(CT_OPEN);

	tc[port].cc_debounce = get_time().val + PD_T_VPDDISABLE;

	return 0;
}

static unsigned int tc_state_ct_disabled_vpd_run(int port)
{
	if (get_time().val > tc[port].cc_debounce) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_disabled_vpd_exit(int port)
{
	return 0;
}

/*
 * CTAttached.VPD
 */
static unsigned int tc_state_ct_attached_vpd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_attached_vpd_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_ct_attached_vpd_entry(int port)
{
	int cc1;
	int cc2;

	CPRINTS("C%d: CTAttached.VPD", port);

	/* Get power from VCONN */
	vpd_vconn_pwr_sel_odl(PWR_VCONN);

	/*
	 * Detect which of the Charge-Through port’s CC1 or CC2
	 * pins is connected through the cable
	 */
	vpd_ct_get_cc(&cc1, &cc2);
	tc[port].ct_cc  = cc_is_rp(cc2) ? 1 : 0;

	/*
	 * 1. Remove or reduce any additional capacitance on the
	 *    Host-side CC port
	 */
	vpd_mcu_cc_en(0);

	/*
	 * 2. Disable the Rp termination advertising 3.0 A on the
	 *    host port’s CC pin
	 */
	vpd_host_set_pull(TYPEC_CC_OPEN, 0);

	/*
	 * 3. Passively multiplex the detected Charge-Through port’s
	 *    CC pin through to the host port’s CC
	 */
	vpd_ct_cc_sel(tc[port].ct_cc ? CT_CC2 : CT_CC1);

	/*
	 * 4. Disable the Rd on the Charge-Through port’s CC1 and CC2
	 *    pins
	 */
	vpd_ct_set_pull(TYPEC_CC_OPEN, 0);

	/*
	 * 5. Connect the Charge-Through port’s VBUS through to the
	 *    host port’s VBUS
	 */
	vpd_vbus_pass_en(1);

	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;

	/* Enable interrupt generation on disconnect */
	pd_rx_enable_monitoring(0);

	return 0;
}

static unsigned int tc_state_ct_attached_vpd_run(int port)
{
	int cc1;
	int cc2;

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_ct_disabled_vpd);
		return 0;
	}

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);
	if (tc[port].ct_cc) {
		if (cc2 == TYPEC_CC_VOLT_OPEN)
			tc[port].new_cc_state = PD_CC_NONE;
		else
			tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	} else {
		if (cc1 == TYPEC_CC_VOLT_OPEN)
			tc[port].new_cc_state = PD_CC_NONE;
		else
			tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	}

	/* Debounce the cc state */
	if (tc[port].new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_VPDCTDD;
		return 0;
	}

	if (get_time().val < tc[port].pd_debounce)
		return 0;

	if (tc[port].cc_state == PD_CC_NONE && !vpd_is_ct_vbus()) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_vpd);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_attached_vpd_exit(int port)
{
	/* Disable interrupt generation on disconnect */
	pd_rx_disable_monitoring(0);

	return 0;
}

/*
 * CTAttachWait.VPD
 */
static unsigned int tc_state_ct_attach_wait_vpd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_attach_wait_vpd_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rp3_ct_rd);
}

static unsigned int tc_state_ct_attach_wait_vpd_entry(int port)
{
	CPRINTS("C%d: CTAttachWait.VPD", port);

	/* Enable PD */
	tc[port].pd_enable = 1;

	/* Enable RX */
	vpd_rx_enable(1);

	tc[port].cc_state = -1;
	tc[port].new_cc_state = -1;

	return 0;
}

static unsigned int tc_state_ct_attach_wait_vpd_run(int port)
{
	int cc1;
	int cc2;

	/* Check CT CC for connection */
	vpd_ct_get_cc(&cc1, &cc2);

	if ((cc_is_rp(cc1) && !cc_is_rp(cc2)) ||
				(!cc_is_rp(cc1) && cc_is_rp(cc2)))
		tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
	else if (!cc_is_rp(cc1) && !cc_is_rp(cc2))
		tc[port].new_cc_state = PD_CC_NONE;
	else
		tc[port].new_cc_state =  -1;

	if (!vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_ct_disabled_vpd);
		return 0;
	}

	/* Debounce the cc state */
	if (tc[port].new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = tc[port].new_cc_state;
		tc[port].cc_debounce = get_time().val +
						PD_T_CC_DEBOUNCE;
		tc[port].pd_debounce = get_time().val +
						PD_T_PD_DEBOUNCE;
		return 0;
	}

	if (get_time().val > tc[port].pd_debounce) {
		/* Debounce complete */
		if (tc[port].cc_state  == PD_CC_NONE) {
			set_state(port, TC_OBJ(port),
					tc_state_ct_unattached_vpd);
			return 0;
		}
	}

	if (get_time().val > tc[port].cc_debounce) {
		/* Debounce complete */
		if (tc[port].cc_state  == PD_CC_DFP_ATTACHED &&
							vpd_is_ct_vbus()) {
			set_state(port, TC_OBJ(port), tc_state_ct_attached_vpd);
			return 0;
		}
	}

	return RUN_SUPER;
}

static unsigned int tc_state_ct_attach_wait_vpd_exit(int port)
{
	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * Super State Host_Rp3_Ct_Rd
 */
static unsigned int tc_state_host_rp3_ct_rd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_host_rp3_ct_rd_sig[sig])(port);

	return SUPER(ret, sig, tc_state_vbus_cc_iso);
}

static unsigned int tc_state_host_rp3_ct_rd_entry(int port)
{
	/* Place RP3A0 on Host CC */
	vpd_host_set_pull(TYPEC_CC_RP, TYPEC_RP_3A0);

	/* Connect Charge-Through Rd */
	vpd_ct_set_pull(TYPEC_CC_RD, 0);

	return 0;
}

static unsigned int tc_state_host_rp3_ct_rd_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_host_rp3_ct_rd_exit(int port)
{
	return 0;
}

/*
 * Super State Host_Rp3_Ct_Rpu
 */
static unsigned int tc_state_host_rp3_ct_rpu(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_host_rp3_ct_rpu_sig[sig])(port);
	return SUPER(ret, sig, tc_state_vbus_cc_iso);
}

static unsigned int tc_state_host_rp3_ct_rpu_entry(int port)
{
	/* Place RP3A0 on Host CC */
	vpd_host_set_pull(TYPEC_CC_RP, TYPEC_RP_3A0);

	/* Place RPUSB on Charge-Through CC */
	vpd_ct_set_pull(TYPEC_CC_RP, TYPEC_RP_USB);

	return 0;
}

static unsigned int tc_state_host_rp3_ct_rpu_run(int port)
{
	return RUN_SUPER;
}

static unsigned int tc_state_host_rp3_ct_rpu_exit(int port)
{
	return 0;
}

static unsigned int get_super_state(int port)
{
	return RUN_SUPER;
}

#endif /* CONFIG_USB_TYPEC_VPD_CT */
