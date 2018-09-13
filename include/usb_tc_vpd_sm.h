/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "vpd_api.h"

/* USB Type-C VPD module */

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
	/* VPD host port cc state */
	enum pd_cc_states host_cc_state;
	enum pd_cc_states host_new_cc_state;
	uint8_t ct_cc;
} tc[CONFIG_USB_PD_PORT_COUNT];

/* Type-C states */
static unsigned int tc_state_disabled(int port, enum signal sig);
static unsigned int tc_state_disabled_entry(int port);
static unsigned int tc_state_disabled_run(int port);
static unsigned int tc_state_disabled_exit(int port);

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

static unsigned int get_super_state(int port);

static const state_sig tc_state_disabled_sig[] = {
	tc_state_disabled_entry,
	tc_state_disabled_run,
	tc_state_disabled_exit
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
 * Unattached.SNK
 */
static unsigned int tc_state_unattached_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_unattached_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_host_rd_ct_rd);
}

static unsigned int tc_state_unattached_snk_entry(int port)
{
	CPRINTS("C%d: Unattached.SNK", port);
	return 0;
}

static unsigned int tc_state_unattached_snk_run(int port)
{
	int host_cc;

	/* Check Host CC for connection */
	vpd_host_get_cc(&host_cc);

	/* Check host CC for connection */
	if (cc_is_rp(host_cc)) {
		set_state(port, TC_OBJ(port), tc_state_attach_wait_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_unattached_snk_exit(int port)
{
	return 0;
}

/*
 * AttachedWait.SNK
 */
static unsigned int tc_state_attach_wait_snk(int port, enum signal sig)
{
	int ret = 0;

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

	return 0;
}

static unsigned int tc_state_attached_snk_run(int port)
{
	/* Has host vbus been and vconn been removed */
	if (!vpd_is_host_vbus() && !vpd_is_vconn()) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	return RUN_SUPER;
}

static unsigned int tc_state_attached_snk_exit(int port)
{
	/* Remove billboard device */
	vpd_present_billboard(BB_NONE);

	/* Disable PD */
	tc[port].pd_enable = 0;

	/* Disable RX */
	vpd_rx_enable(0);

	return 0;
}

/*
 * Super State HOST_OPEN_CT_OPEN
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
 * Super State VBUS_CC_ISO
 */
static unsigned int tc_state_vbus_cc_iso(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_vbus_cc_iso_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_vbus_cc_iso_entry(int port)
{
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

static unsigned int get_super_state(int port)
{
	return RUN_SUPER;
}

#endif /*__CROS_EC_USB_TC_VPD_H */
