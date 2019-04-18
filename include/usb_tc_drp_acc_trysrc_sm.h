/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_emsg.h"

/*
 * USB Type-C DRP with Accessory and Try.SRC module
 *   See Figure 4-16 in Release 1.4 of USB Type-C Spec.
 */

#ifndef __CROS_EC_USB_TC_DRP_ACC_TRYSRC_H
#define __CROS_EC_USB_TC_DRP_ACC_TRYSRC_H

/* Type-C Layer Flags */
#define TC_FLAGS_VCONN_ON                 BIT(0)
#define TC_FLAGS_PARTNER_USB_COMM         BIT(1)
#define TC_FLAGS_TS_DTS_PARTNER           BIT(2)

/* Port default state at startup */
#define TC_DEFAULT_STATE(port) tc_state_unattached_snk

static struct type_c {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	/* state id */
	enum typec_state_id state_id;
	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t polarity;
	/* port flags, see TC_FLAGS_* */
	uint32_t flags;
	/* event timeout */
	uint64_t evt_timeout;
	/* state machine event */
	int evt;
	/* Time a port shall wait before it can determine it is attached */
	uint64_t cc_debounce;
	/*
	 * Time a Sink port shall wait before it can determine it is detached
	 * due to the potential for USB PD signaling on CC as described in
	 * the state definitions.
	 */
	uint64_t pd_debounce;
#ifdef CONFIG_USB_PD_TRY_SRC
	/*
	 * Time a port shall wait before it can determine it is
	 * re-attached during the try-wait process.
	 */
	uint64_t try_wait_debounce;
#endif
	/* The cc state */
	enum pd_cc_states cc_state;
	/* Role toggle timer */
	uint64_t next_role_swap;
	/* Generic timer */
	uint64_t timeout;
	/* Type-C current */
	typec_current_t typec_curr;
	/* Type-C current change */
	typec_current_t typec_curr_change;
	/* Attached ChromeOS device id, RW hash, and current RO / RW image */
	uint16_t dev_id;
	uint32_t dev_rw_hash[PD_RW_HASH_SIZE/4];
	enum ec_current_image current_image;
} tc[CONFIG_USB_PD_PORT_COUNT];

/* Port dual-role state */
enum pd_dual_role_states drp_state[CONFIG_USB_PD_PORT_COUNT] = {
	[0 ... (CONFIG_USB_PD_PORT_COUNT - 1)] =
		CONFIG_USB_PD_INITIAL_DRP_STATE};

/*
 * 4 entry rw_hash table of type-C devices that AP has firmware updates for.
 */
#ifdef CONFIG_COMMON_RUNTIME
#define RW_HASH_ENTRIES 4
static struct ec_params_usb_pd_rw_hash_entry rw_hash_table[RW_HASH_ENTRIES];
#endif
#ifdef CONFIG_USBC_VCONN
static void set_vconn(int port, int enable);
#endif
static void tc_set_data_role(int port, int role);

#if defined(CONFIG_CHARGE_MANAGER)
static typec_current_t get_typec_current_limit(int polarity, int cc1, int cc2);
#endif

#ifdef CONFIG_USB_PD_TRY_SRC
/* Enable variable for Try.SRC states */
static uint8_t pd_try_src_enable;
static void pd_update_try_source(void);
#endif

/*
 * Type-C states
 */
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

static unsigned int tc_state_attach_wait_snk(int port, enum signal sig);
static unsigned int tc_state_attach_wait_snk_entry(int port);
static unsigned int tc_state_attach_wait_snk_run(int port);

static unsigned int tc_state_attached_snk(int port, enum signal sig);
static unsigned int tc_state_attached_snk_entry(int port);
static unsigned int tc_state_attached_snk_run(int port);
static unsigned int tc_state_attached_snk_exit(int port);

static unsigned int tc_state_dbg_acc_snk(int port, enum signal sig);
static unsigned int tc_state_dbg_acc_snk_entry(int port);
static unsigned int tc_state_dbg_acc_snk_run(int port);

#ifdef CONFIG_USB_PD_TRY_SRC
static unsigned int tc_state_try_src(int port, enum signal sig);
static unsigned int tc_state_try_src_entry(int port);
static unsigned int tc_state_try_src_run(int port);
#endif

static unsigned int tc_state_unattached_src(int port, enum signal sig);
static unsigned int tc_state_unattached_src_entry(int port);
static unsigned int tc_state_unattached_src_run(int port);

static unsigned int tc_state_attach_wait_src(int port, enum signal sig);
static unsigned int tc_state_attach_wait_src_entry(int port);
static unsigned int tc_state_attach_wait_src_run(int port);

static unsigned int tc_state_try_wait_snk(int port, enum signal sig);
static unsigned int tc_state_try_wait_snk_entry(int port);
static unsigned int tc_state_try_wait_snk_run(int port);

static unsigned int tc_state_attached_src(int port, enum signal sig);
static unsigned int tc_state_attached_src_entry(int port);
static unsigned int tc_state_attached_src_run(int port);
static unsigned int tc_state_attached_src_exit(int port);

static unsigned int tc_state_audio_acc(int port, enum signal sig);
static unsigned int tc_state_audio_acc_entry(int port);
static unsigned int tc_state_audio_acc_run(int port);
static unsigned int tc_state_audio_acc_exit(int port);

/* Super States */

static unsigned int tc_state_cc_rd(int port, enum signal sig);
static unsigned int tc_state_cc_rd_entry(int port);

static unsigned int tc_state_cc_rp(int port, enum signal sig);
static unsigned int tc_state_cc_rp_entry(int port);

static unsigned int tc_state_cc_open(int port, enum signal sig);
static unsigned int tc_state_cc_open_entry(int port);

static unsigned int do_nothing(int port);
static unsigned int get_super_state(int port);

#ifdef CONFIG_USB_PD_TRY_SRC
static const state_sig tc_state_try_src_sig[] = {
	tc_state_try_src_entry,
	tc_state_try_src_run,
	do_nothing,
	get_super_state
};
#endif

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
	do_nothing,
	get_super_state
};

static const state_sig tc_state_attach_wait_snk_sig[] = {
	tc_state_attach_wait_snk_entry,
	tc_state_attach_wait_snk_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_attached_snk_sig[] = {
	tc_state_attached_snk_entry,
	tc_state_attached_snk_run,
	tc_state_attached_snk_exit,
	get_super_state
};

static const state_sig tc_state_dbg_acc_snk_sig[] = {
	tc_state_dbg_acc_snk_entry,
	tc_state_dbg_acc_snk_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_unattached_src_sig[] = {
	tc_state_unattached_src_entry,
	tc_state_unattached_src_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_attach_wait_src_sig[] = {
	tc_state_attach_wait_src_entry,
	tc_state_attach_wait_src_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_try_wait_snk_sig[] = {
	tc_state_try_wait_snk_entry,
	tc_state_try_wait_snk_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_attached_src_sig[] = {
	tc_state_attached_src_entry,
	tc_state_attached_src_run,
	tc_state_attached_src_exit,
	get_super_state
};

static const state_sig tc_state_audio_acc_sig[] = {
	tc_state_audio_acc_entry,
	tc_state_audio_acc_run,
	tc_state_audio_acc_exit,
	get_super_state
};

static const state_sig tc_state_cc_rd_sig[] = {
	tc_state_cc_rd_entry,
	do_nothing,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_cc_rp_sig[] = {
	tc_state_cc_rp_entry,
	do_nothing,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_cc_open_sig[] = {
	tc_state_cc_open_entry,
	do_nothing,
	do_nothing,
	get_super_state
};

/*
 * Public Functions
 *
 * NOTE: Functions prefixed with pd_ are defined in usb_pd.h
 *       Functions prefixed with tc_ are defined int usb_tc_sm.h
 */

#if !defined(CONFIG_USB_PRL_SM)

/*
 * These pd_ functions are implemented in common/usb_prl_sm.c
 */

void pd_transmit_complete(int port, int status)
{
	/* DO NOTHING */
}

void pd_execute_hard_reset(int port)
{
	/* DO NOTHING */
}

void pd_set_vbus_discharge(int port, int enable)
{
	/* DO NOTHING */
}

uint16_t pd_get_identity_vid(int port)
{
	/* DO NOTHING */
	return 0;
}

#endif

/*
 * TODO(shurst): Implement the pd_ functions below when PD
 * functionality is added
 */
void pd_update_contract(int port)
{
	/* DO NOTHING */
}

void pd_set_new_power_request(int port)
{
	/* DO NOTHING */
}

void pd_request_power_swap(int port)
{
	/* DO NOTHING */
}

#ifdef CONFIG_COMMON_RUNTIME
void pd_set_suspend(int port, int enable)
{
	set_state(port, TC_OBJ(port),
		(enable) ? tc_state_disabled : TC_DEFAULT_STATE(port));
}

int pd_is_port_enabled(int port)
{
	return !(tc[port].state_id == DISABLED);
}
#endif

int pd_get_polarity(int port)
{
	return tc[port].polarity;
}

int pd_get_role(int port)
{
	return tc[port].data_role;
}

int pd_is_vbus_present(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_VBUS_DETECT_TCPC))
		return tcpm_get_vbus_level(port);
	else
		return pd_snk_is_vbus_provided(port);
}

/* Return flag for pd state is connected */
int pd_is_connected(int port)
{
	return ((tc[port].obj.task_state == tc_state_attached_snk) ||
			(tc[port].obj.task_state == tc_state_attached_src));
}

int pd_dev_store_rw_hash(int port, uint16_t dev_id, uint32_t *rw_hash,
			uint32_t current_image)
{
#ifdef CONFIG_COMMON_RUNTIME
	int i;
#endif

	tc[port].dev_id = dev_id;
	memcpy(tc[port].dev_rw_hash, rw_hash, PD_RW_HASH_SIZE);
#ifdef CONFIG_CMD_PD_DEV_DUMP_INFO
	if (debug_level >= 2)
		pd_dev_dump_info(dev_id, (uint8_t *)rw_hash);
#endif
	tc[port].current_image = current_image;

#ifdef CONFIG_COMMON_RUNTIME
	/* Search table for matching device / hash */
	for (i = 0; i < RW_HASH_ENTRIES; i++)
		if (dev_id == rw_hash_table[i].dev_id)
			return !memcmp(rw_hash,
				       rw_hash_table[i].dev_rw_hash,
				       PD_RW_HASH_SIZE);
#endif
	return 0;
}

void tc_src_power_off(int port)
{
	if (tc[port].state_id == ATTACHED_SRC) {
		/* Remove VBUS */
		pd_power_supply_reset(port);
		pd_set_input_current_limit(port, 0, 0);

		if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
			typec_set_input_current_limit(port, 0, 0);
			charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);
		}
	}
}

void tc_start_error_recovery(int port)
{
	/*
	 * Async. function call:
	 *   The port should transition to the ErrorRecovery state
	 *   from any other state when directed.
	 */
	set_state(port, TC_OBJ(port), tc_state_error_recovery);
}

void tc_state_init(int port)
{
	int res = 0;
	sm_state this_state;

	res = tc_restart_tcpc(port);

	CPRINTS("TCPC p%d init %s", port, res ? "failed" : "ready");
	this_state = res ? tc_state_disabled : TC_DEFAULT_STATE(port);

	init_state(port, TC_OBJ(port), this_state);

	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		/* Initialize USB mux to its default state */
		usb_mux_init(port);
#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
		tcpm_select_rp_value(port,
				CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT);
#else
		tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
#endif
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		/* Initialize PD and type-C supplier current limits to 0 */
		pd_set_input_current_limit(port, 0, 0);
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_update_dualrole(port, CAP_UNKNOWN);
	}

	/* Assume port partner is USB COMMs capable */
	/* TODO(shurt): remove this assumption when PD is implemented */
	tc[port].flags = TC_FLAGS_PARTNER_USB_COMM;
	tc[port].evt_timeout = 5*MSEC;
}

/*
 * Private Functions
 */

static void tc_event_check(int port, int evt)
{
	/* NO EVENTS TO CHECK */
}

/*
 * CC values for regular sources and Debug sources (aka DTS)
 *
 * Source type  Mode of Operation   CC1    CC2
 * ---------------------------------------------
 * Regular      Default USB Power   RpUSB  Open
 * Regular      USB-C @ 1.5 A       Rp1A5  Open
 * Regular      USB-C @ 3 A         Rp3A0  Open
 * DTS          Default USB Power   Rp3A0  Rp1A5
 * DTS          USB-C @ 1.5 A       Rp1A5  RpUSB
 * DTS          USB-C @ 3 A         Rp3A0  RpUSB
 */

static void tc_set_data_role(int port, int role)
{
	tc[port].data_role = role;

	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		set_usb_mux_with_current_data_role(port);

	/* Notify TCPC of role update */
	tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);
}

#ifdef CONFIG_USB_PD_TRY_SRC
static void pd_update_try_source(void)
{
	int i;
	int try_src = 0;

#ifndef CONFIG_CHARGER
	int batt_soc = board_get_battery_soc();
#else
	int batt_soc = charge_get_percent();
#endif

	try_src = 0;
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		try_src |= drp_state[i] == PD_DRP_TOGGLE_ON;

	/*
	 * Enable try source when dual-role toggling AND battery is present
	 * and at some minimum percentage.
	 */
	pd_try_src_enable = try_src &&
			    batt_soc >= CONFIG_USB_PD_TRY_SRC_MIN_BATT_SOC;
#if defined(CONFIG_BATTERY_PRESENT_CUSTOM) || \
			defined(CONFIG_BATTERY_PRESENT_GPIO)
	/*
	 * When battery is cutoff in ship mode it may not be reliable to
	 * check if battery is present with its state of charge.
	 * Also check if battery is initialized and ready to provide power.
	 */
	pd_try_src_enable &= (battery_is_present() == BP_YES);
#endif

}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, pd_update_try_source, HOOK_PRIO_DEFAULT);
#endif /* CONFIG_USB_PD_TRY_SRC */

static inline void pd_set_dual_role_no_wakeup(int port,
					enum pd_dual_role_states state)
{
	drp_state[port] = state;

	if (IS_ENABLED(CONFIG_USB_PD_TRY_SRC))
		pd_update_try_source();
}

#ifdef CONFIG_CMD_PD_DEV_DUMP_INFO
static inline void pd_dev_dump_info(uint16_t dev_id, uint8_t *hash)
{
	int j;

	ccprintf("DevId:%d.%d Hash:", HW_DEV_ID_MAJ(dev_id),
		 HW_DEV_ID_MIN(dev_id));
	for (j = 0; j < PD_RW_HASH_SIZE; j += 4) {
		ccprintf(" 0x%02x%02x%02x%02x", hash[j + 3], hash[j + 2],
			 hash[j + 1], hash[j]);
	}
	ccprintf("\n");
}
#endif /* CONFIG_CMD_PD_DEV_DUMP_INFO */

#if defined(CONFIG_CHARGE_MANAGER)
/*
 * Returns type C current limit (mA) based upon cc_voltage (mV).
 */
static typec_current_t get_typec_current_limit(int polarity, int cc1, int cc2)
{
	typec_current_t charge;
	int cc = polarity ? cc2 : cc1;
	int cc_alt = polarity ? cc1 : cc2;

	if (cc == TYPEC_CC_VOLT_RP_3_0 && cc_alt != TYPEC_CC_VOLT_RP_1_5)
		charge = 3000;
	else if (cc == TYPEC_CC_VOLT_RP_1_5)
		charge = 1500;
	else if (cc == TYPEC_CC_VOLT_RP_DEF)
		charge = 500;
	else
		charge = 0;

	if (cc_is_rp(cc_alt))
		charge |= TYPEC_CURRENT_DTS_MASK;

	return charge;
}
#endif

#ifdef CONFIG_USBC_VCONN
static void set_vconn(int port, int enable)
{
	if (enable)
		TC_SET_FLAG(port, TC_FLAGS_VCONN_ON);
	else
		TC_CLR_FLAG(port, TC_FLAGS_VCONN_ON);

	/*
	 * We always need to tell the TCPC to enable Vconn first, otherwise some
	 * TCPCs get confused and think the CC line is in over voltage mode and
	 * immediately disconnects. If there is a PPC, both devices will
	 * potentially source Vconn, but that should be okay since Vconn has
	 * "make before break" electrical requirements when swapping anyway.
	 */
	tcpm_set_vconn(port, enable);

#ifdef CONFIG_USBC_PPC_VCONN
	ppc_set_vconn(port, enable);
#endif
}
#endif /* defined(CONFIG_USBC_VCONN) */

#ifdef CONFIG_USB_PD_TCPM_TCPCI
static uint32_t pd_ports_to_resume;
static void resume_pd_port(void)
{
	uint32_t port;
	uint32_t suspended_ports = atomic_read_clear(&pd_ports_to_resume);

	while (suspended_ports) {
		port = __builtin_ctz(suspended_ports);
		suspended_ports &= ~(1 << port);
		pd_set_suspend(port, 0);
	}
}
DECLARE_DEFERRED(resume_pd_port);

void pd_deferred_resume(int port)
{
	atomic_or(&pd_ports_to_resume, 1 << port);
	hook_call_deferred(&resume_pd_port_data, SECOND);
}
#endif  /* CONFIG_USB_PD_DEFERRED_RESUME */

/*
 * HOST COMMANDS
 */
static int hc_pd_ports(struct host_cmd_handler_args *args)
{
	struct ec_response_usb_pd_ports *r = args->response;

	r->num_ports = CONFIG_USB_PD_PORT_COUNT;
	args->response_size = sizeof(*r);

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_PORTS,
			hc_pd_ports,
			EC_VER_MASK(0));

static int hc_remote_rw_hash_entry(struct host_cmd_handler_args *args)
{
	int i;
	int idx = 0;
	int found = 0;
	const struct ec_params_usb_pd_rw_hash_entry *p = args->params;
	static int rw_hash_next_idx;

	if (!p->dev_id)
		return EC_RES_INVALID_PARAM;

	for (i = 0; i < RW_HASH_ENTRIES; i++) {
		if (p->dev_id == rw_hash_table[i].dev_id) {
			idx = i;
			found = 1;
			break;
		}
	}

	if (!found) {
		idx = rw_hash_next_idx;
		rw_hash_next_idx = rw_hash_next_idx + 1;
		if (rw_hash_next_idx == RW_HASH_ENTRIES)
			rw_hash_next_idx = 0;
	}

	memcpy(&rw_hash_table[idx], p, sizeof(*p));

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_RW_HASH_ENTRY,
			hc_remote_rw_hash_entry,
			EC_VER_MASK(0));

static int hc_remote_pd_dev_info(struct host_cmd_handler_args *args)
{
	const uint8_t *port = args->params;
	struct ec_params_usb_pd_rw_hash_entry *r = args->response;

	if (*port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	r->dev_id = tc[*port].dev_id;

	if (r->dev_id)
		memcpy(r->dev_rw_hash, tc[*port].dev_rw_hash, PD_RW_HASH_SIZE);

	r->current_image = tc[*port].current_image;

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_USB_PD_DEV_INFO,
			hc_remote_pd_dev_info,
			EC_VER_MASK(0));


/*
 * TYPE-C State Implementations
 */

/**
 * Disabled
 *
 * Super State Entry Actions:
 *   Remove the terminations from CC
 */
static unsigned int tc_state_disabled(int port, enum signal sig)
{
	int ret = 0;

	ret = (*tc_state_disabled_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_open);
}

static unsigned int tc_state_disabled_entry(int port)
{
	tc[port].state_id = DISABLED;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	return 0;
}

static unsigned int tc_state_disabled_run(int port)
{
	task_wait_event(-1);
	return RUN_SUPER;
}

static unsigned int tc_state_disabled_exit(int port)
{
	if (!IS_ENABLED(CONFIG_USB_PD_TCPC)) {
		if (tc_restart_tcpc(port) != 0) {
			CPRINTS("TCPC p%d restart failed!", port);
			return 0;
		}
	}

	CPRINTS("TCPC p%d resumed!", port);

	return 0;
}

/**
 * ErrorRecovery
 *
 * Super State Entry Actions:
 *   Remove the terminations from CC
 */
static unsigned int tc_state_error_recovery(int port, enum signal sig)
{
	int ret = 0;

	ret = (*tc_state_error_recovery_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_open);
}

static unsigned int tc_state_error_recovery_entry(int port)
{
	tc[port].state_id = ERROR_RECOVERY;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].timeout = get_time().val + PD_T_ERROR_RECOVERY;
	return 0;
}

static unsigned int tc_state_error_recovery_run(int port)
{
	if (tc[port].timeout > 0 && get_time().val > tc[port].timeout) {
		tc[port].timeout = 0;
		tc_state_init(port);
	}

	return 0;
}

static unsigned int tc_state_error_recovery_exit(int port)
{
	return 0;
}

/**
 * Unattached.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
static unsigned int tc_state_unattached_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_unattached_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rd);
}

static unsigned int tc_state_unattached_snk_entry(int port)
{
	tc[port].state_id = UNATTACHED_SNK;
	if (tc[port].obj.last_state != tc_state_unattached_src)
		CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charge_manager_update_dualrole(port, CAP_UNKNOWN);

	/*
	 * Indicate that the port is disconnected so the board
	 * can restore state from any previous data swap.
	 */
	pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
	tc[port].next_role_swap = get_time().val + PD_T_DRP_SNK;

	return 0;
}

static unsigned int tc_state_unattached_snk_run(int port)
{
	int cc1;
	int cc2;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	/*
	 * The port shall transition to AttachWait.SNK when a Source
	 * connection is detected, as indicated by the SNK.Rp state
	 * on at least one of its CC pins.
	 *
	 * A DRP shall transition to Unattached.SRC within tDRPTransition
	 * after the state of both CC pins is SNK.Open for
	 * tDRP − dcSRC.DRP ∙ tDRP.
	 */
	if (cc1 != TYPEC_CC_VOLT_OPEN || cc2 != TYPEC_CC_VOLT_OPEN) {
		/* Connection Detected */
		set_state(port, TC_OBJ(port), tc_state_attach_wait_snk);
	} else if (get_time().val > tc[port].next_role_swap) {
		/* DRP Toggle */
		set_state(port, TC_OBJ(port), tc_state_unattached_src);
	}

	return 0;
}

/**
 * AttachWait.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
static unsigned int tc_state_attach_wait_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attach_wait_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rd);
}

static unsigned int tc_state_attach_wait_snk_entry(int port)
{
	tc[port].state_id = ATTACH_WAIT_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].cc_state = PD_CC_UNSET;
	return 0;
}

static unsigned int tc_state_attach_wait_snk_run(int port)
{
	int cc1;
	int cc2;
	int new_cc_state;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (cc_is_rp(cc1) && cc_is_rp(cc2))
		new_cc_state = PD_CC_DEBUG_ACC;
	else if (cc_is_rp(cc1) || cc_is_rp(cc2))
		new_cc_state = PD_CC_DFP_ATTACHED;
	else
		new_cc_state = PD_CC_NONE;

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		tc[port].pd_debounce = get_time().val + PD_T_PD_DEBOUNCE;
		tc[port].cc_state = new_cc_state;
		return 0;
	}

	/*
	 * A DRP shall transition to Unattached.SNK when the state of both
	 * the CC1 and CC2 pins is SNK.Open for at least tPDDebounce.
	 */
	if (new_cc_state == PD_CC_NONE &&
				get_time().val > tc[port].pd_debounce) {
		/* We are detached */
		set_state(port, TC_OBJ(port), tc_state_unattached_src);
		return 0;
	}

	/* Wait for CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	/*
	 * The port shall transition to Attached.SNK after the state of only one
	 * of the CC1 or CC2 pins is SNK.Rp for at least tCCDebounce and VBUS is
	 * detected.
	 *
	 * A DRP that strongly prefers the Source role may optionally transition
	 * to Try.SRC instead of Attached.SNK when the state of only one CC pin
	 * has been SNK.Rp for at least tCCDebounce and VBUS is detected.
	 *
	 * If the port supports Debug Accessory Mode, the port shall transition
	 * to DebugAccessory.SNK if the state of both the CC1 and CC2 pins is
	 * SNK.Rp for at least tCCDebounce and VBUS is detected.
	 */
	if (pd_is_vbus_present(port)) {
		if (new_cc_state == PD_CC_DFP_ATTACHED) {
#ifdef CONFIG_USB_PD_TRY_SRC
			if (pd_try_src_enable)
				set_state(port, TC_OBJ(port), tc_state_try_src);
			else
#endif
				set_state(port, TC_OBJ(port),
							tc_state_attached_snk);
		}
		/* new_cc_state == PD_CC_DEBUG_ACC */
		else {
			TC_SET_FLAG(port, TC_FLAGS_TS_DTS_PARTNER);
			set_state(port, TC_OBJ(port),
						tc_state_dbg_acc_snk);
		}
	}

	return RUN_SUPER;
}

/**
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
	int cc1;
	int cc2;

	tc[port].state_id = ATTACHED_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/* Get connector orientation */
	tcpm_get_cc(port, &cc1, &cc2);
	tc[port].polarity = get_snk_polarity(cc1, cc2);
	set_polarity(port, tc[port].polarity);

	/*
	 * Initial data role for sink is UFP
	 * This also sets the usb mux
	 */
	tc_set_data_role(port, PD_ROLE_UFP);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		tc[port].typec_curr =
			get_typec_current_limit(tc[port].polarity, cc1, cc2);
		typec_set_input_current_limit(port, tc[port].typec_curr,
						TYPE_C_VOLTAGE);
		charge_manager_update_dualrole(port, CAP_DEDICATED);
		tc[port].cc_state = (tc[port].polarity) ? cc2 : cc1;
	}

	tc[port].timeout = 0;
	tc[port].cc_debounce = 0;
	return 0;
}

static unsigned int tc_state_attached_snk_run(int port)
{
	/* Detach detection */
	if (!pd_is_vbus_present(port)) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	/*
	 * Sink Power Sub-State
	 */
	{
		int cc1;
		int cc2;
		int new_cc_state;

		tcpm_get_cc(port, &cc1, &cc2);

		if (cc1 == TYPEC_CC_VOLT_RP_DEF ||
						cc2 == TYPEC_CC_VOLT_RP_DEF)
			new_cc_state = TYPEC_CC_VOLT_RP_DEF;
		else if (cc1 == TYPEC_CC_VOLT_RP_1_5 ||
						cc2 == TYPEC_CC_VOLT_RP_1_5)
			new_cc_state = TYPEC_CC_VOLT_RP_1_5;
		else if (cc1 == TYPEC_CC_VOLT_RP_3_0 ||
						cc2 == TYPEC_CC_VOLT_RP_3_0)
			new_cc_state = TYPEC_CC_VOLT_RP_3_0;
		else
			new_cc_state = TYPEC_CC_VOLT_OPEN;

		/* Debounce the cc state */
		if (new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = new_cc_state;
			tc[port].cc_debounce =
					get_time().val + PD_T_RP_VALUE_CHANGE;
			return 0;
		}

		if (tc[port].cc_debounce == 0 ||
					get_time().val < tc[port].cc_debounce)
			return 0;

		tc[port].cc_debounce = 0;

		if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
			tc[port].typec_curr =
				get_typec_current_limit(tc[port].polarity,
								cc1, cc2);

			typec_set_input_current_limit(port,
				tc[port].typec_curr, TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port, CAP_DEDICATED);
		}
	}

	return 0;
}

static unsigned int tc_state_attached_snk_exit(int port)
{
	/* Stop drawing power */
	pd_set_input_current_limit(port, 0, 0);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port,
				CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
	}

	return 0;
}

/*
 * TODO(shurst): DO WE NEED UnorientedDebugAccessory.SRC
 */

#if 0
/**
 * UnorientedDebugAccessory.SRC
 *
 * Super State Entry Actions:
 *  Vconn Off
 *  Place Rp on CC
 *  Set power role to SOURCE
 */
static unsigned int tc_state_unoriented_dbg_acc_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_unoriented_dbg_acc_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_unoriented_dbg_acc_src_entry(int port)
{
	int cc1;
	int cc2;

	tc[port].state_id = UNORIENTED_DEBUG_ACCESSORY_SRC;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/* Get connector orientation */
	tcpm_get_cc(port, &cc1, &cc2);
	tc[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);
	set_polarity(port, tc[port].polarity);

	/* Enable VBUS */
	pd_set_power_supply_ready(port);

	/*
	 *      IS THIS NEEDED?
	 * SETUP FOR UNORIENTED DEBUG
	 */

	return 0;
}

static unsigned int tc_state_unoriented_dbg_acc_src_run(int port)
{
	int cc1;
	int cc2;

#if 1
	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (tc[port].polarity)
		cc1 = cc2;

	/*
	 * A DRP, the port shall transition to Unattached.SNK when the
	 * SRC.Open state is detected on either the CC1 or CC2 pin.
	 */
	if (cc1 == TYPEC_CC_VOLT_OPEN) {
		/* Remove VBUS */
		pd_power_supply_reset(port);
		pd_set_input_current_limit(port, 0, 0);
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);

		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
	}
#else
	/*
	 * The port shall transition to OrientedDebugAccessory.SRC state if
	 * orientation is required
	 */
	set_state(port, TC_OBJ(port), tc_state_oriented_dbg_acc_src);
#endif
	return 0;
}

/**
 * OrientedDebugAccessory.SRC
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rp on CC
 *   Set power role to SOURCE
 */

static unsigned int tc_state_oriented_dbg_acc_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_oriented_dbg_acc_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_oriented_dbg_acc_src_entry(int port)
{
	tc[port].state_id = ORIENTED_DEBUG_ACCESSORY_SRC;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/*
	 *      IS THIS NEEDED?
	 * SETUP FOR ORIENTED DEBUG
	 */

	return 0;
}

static unsigned int tc_state_oriented_dbg_acc_src_run(int port)
{
	return 0;
}

#endif

/**
 * Audio Accessory
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rp on CC
 *   Set power role to SOURCE
 */
static unsigned int tc_state_audio_acc(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_audio_acc_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_audio_acc_entry(int port)
{
	tc[port].state_id = AUDIO_ACCESSORY;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].cc_state = PD_CC_UNSET;

	return 0;
}

static unsigned int tc_state_audio_acc_run(int port)
{
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		/*
		 * A port that sinks current from the audio accessory
		 * over VBUS shall not draw more than 500 mA.
		 */
		if (pd_is_vbus_present(port)) {
			typec_set_input_current_limit(port,
				TYPE_C_AUDIO_ACC_CURRENT, TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port, CAP_DEDICATED);
		} else {
			typec_set_input_current_limit(port, 0, 0);
			charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);
		}
	}

	{
		int cc1;
		int cc2;
		int new_cc_state;

		/* Check for connection */
		tcpm_get_cc(port, &cc1, &cc2);

		if (cc1 == TYPEC_CC_VOLT_OPEN || cc2 == TYPEC_CC_VOLT_OPEN)
			new_cc_state = PD_CC_NONE;
		else
			new_cc_state = PD_CC_UFP_ATTACHED;

		/* Debounce the cc state */
		if (new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = new_cc_state;
			tc[port].cc_debounce =
					get_time().val + PD_T_CC_DEBOUNCE;
			return 0;
		}

		/*
		 * A DRP shall transition to Unattached.SRC when the state of
		 * the monitored CC1 or CC2 pin(s) is SRC.Open for at least
		 * tCCDebounce.
		 */
		if ((get_time().val > tc[port].cc_debounce) &&
				new_cc_state == PD_CC_NONE) {
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
		}
	}

	return 0;
}

static unsigned int tc_state_audio_acc_exit(int port)
{
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);
	}

	return 0;
}

/**
 * Debug Accessory.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
static unsigned int tc_state_dbg_acc_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_dbg_acc_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rd);
}

static unsigned int tc_state_dbg_acc_snk_entry(int port)
{
	tc[port].state_id = DEBUG_ACCESSORY_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/*
	 * SETUP FOR DEBUG ACCESSORY
	 */

	return 0;
}

static unsigned int tc_state_dbg_acc_snk_run(int port)
{
	if (!pd_is_vbus_present(port))
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);

	return 0;
}

/**
 * Unattached.SRC
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rp on CC
 *   Set power role to SOURCE
 */
static unsigned int tc_state_unattached_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_unattached_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_unattached_src_entry(int port)
{
	tc[port].state_id = UNATTACHED_SRC;
	if (tc[port].obj.last_state != tc_state_unattached_snk)
		CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charge_manager_update_dualrole(port, CAP_UNKNOWN);

	tc_set_data_role(port, PD_ROLE_DFP);

	/*
	 * Indicate that the port is disconnected so the board
	 * can restore state from any previous data swap.
	 */
	pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);

	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
						     tc[port].polarity);

	tc[port].next_role_swap = get_time().val + PD_T_DRP_SRC;

	return 0;
}

static unsigned int tc_state_unattached_src_run(int port)
{
	int cc1;
	int cc2;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	/*
	 * Transition to AttachWait.SRC when VBUS is vSafe0V and:
	 *   1) The SRC.Rd state is detected on either CC1 or CC2 pin or
	 *   2) The SRC.Ra state is detected on both the CC1 and CC2 pins.
	 *
	 * A DRP shall transition to Unattached.SNK within tDRPTransition
	 * after dcSRC.DRP ∙ tDRP
	 */
	if ((cc1 == TYPEC_CC_VOLT_RD || cc2 == TYPEC_CC_VOLT_RD) ||
			(cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_RA))
		set_state(port, TC_OBJ(port), tc_state_attach_wait_src);
	else if (get_time().val > tc[port].next_role_swap)
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);

	return RUN_SUPER;
}

/**
 * AttachWait.SRC
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rp on CC
 *   Set power role to SOURCE
 */
static unsigned int tc_state_attach_wait_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_attach_wait_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_attach_wait_src_entry(int port)
{
	tc[port].state_id = ATTACH_WAIT_SRC;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].cc_state = PD_CC_UNSET;

	return 0;
}

static unsigned int tc_state_attach_wait_src_run(int port)
{
	int cc1;
	int cc2;
	int new_cc_state;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_RD) {
		/* Debug accessory */
		new_cc_state = PD_CC_DEBUG_ACC;
	} else if (cc1 == TYPEC_CC_VOLT_RD ||
				cc2 == TYPEC_CC_VOLT_RD) {
		/* UFP attached */
		new_cc_state = PD_CC_UFP_ATTACHED;
	} else if (cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_RA) {
		/* Audio accessory */
		new_cc_state = PD_CC_AUDIO_ACC;
	} else {
		/* No UFP */
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		tc[port].cc_state = new_cc_state;
		return 0;
	}

	/* Wait for CC debounce */
	if (get_time().val < tc[port].cc_debounce)
		return 0;

	/*
	 * The port shall transition to Attached.SRC when VBUS is at vSafe0V
	 * and the SRC.Rd state is detected on exactly one of the CC1 or CC2
	 * pins for at least tCCDebounce.
	 *
	 * If the port supports Debug Accessory Mode, it shall transition to
	 * UnorientedDebugAccessory.SRC when VBUS is at vSafe0V and the SRC.Rd
	 * state is detected on both the CC1 and CC2 pins for at least
	 * tCCDebounce.
	 */
	if (!pd_is_vbus_present(port)) {
		if (new_cc_state == PD_CC_UFP_ATTACHED) {
			set_state(port, TC_OBJ(port), tc_state_attached_src);
			return 0;
		}
/*
 * TODO(shurst): DO WE NEED tc_state_unoriented_dbg_acc_src
 */
#if 0
		else if (new_cc_state == PD_CC_DEBUG_ACC) {
			set_state(port, TC_OBJ(port),
					tc_state_unoriented_dbg_acc_src);
			return 0;
		}
#endif
	}

	/*
	 * If the port supports Audio Adapter Accessory Mode, it shall
	 * transition to AudioAccessory when the SRC.Ra state is detected on
	 * both the CC1 and CC2 pins for at least tCCDebounce.
	 */
	if (new_cc_state == PD_CC_AUDIO_ACC)
		set_state(port, TC_OBJ(port), tc_state_audio_acc);

	return 0;
}

/**
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
	int cc1;
	int cc2;

	tc[port].state_id = ATTACHED_SRC;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/* Get connector orientation */
	tcpm_get_cc(port, &cc1, &cc2);
	tc[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);
	set_polarity(port, tc[port].polarity);

	/*
	 * Initial data role for sink is DFP
	 * This also sets the usb mux
	 */
	tc_set_data_role(port, PD_ROLE_DFP);

	/*
	 * Start sourcing Vconn before Vbus to ensure
	 * we are within USB Type-C Spec 1.4 tVconnON
	 */
	if (IS_ENABLED(CONFIG_USBC_VCONN))
		set_vconn(port, 1);

	/* Enable VBUS */
	if (pd_set_power_supply_ready(port)) {
		/* Stop sourcing Vconn if Vbus failed */
		if (IS_ENABLED(CONFIG_USBC_VCONN))
			set_vconn(port, 0);

		if (IS_ENABLED(CONFIG_USBC_SS_MUX))
			usb_mux_set(port, TYPEC_MUX_NONE,
				USB_SWITCH_DISCONNECT, tc[port].polarity);
	}

	return 0;
}

static unsigned int tc_state_attached_src_run(int port)
{
	int cc1;
	int cc2;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (tc[port].polarity)
		cc1 = cc2;

	/*
	 * When the SRC.Open state is detected on the monitored CC pin, a DRP
	 * shall transition to Unattached.SNK unless it strongly prefers the
	 * Source role. In that case, it shall transition to TryWait.SNK.
	 * This transition to TryWait.SNK is needed so that two devices that
	 * both prefer the Source role do not loop endlessly between Source
	 * and Sink. In other words, a DRP that would enter Try.SRC from
	 * AttachWait.SNK shall enter TryWait.SNK for a Sink detach from
	 * Attached.SRC.
	 */
	if (cc1 == TYPEC_CC_VOLT_OPEN) {
		set_state(port, TC_OBJ(port), tc_state_try_wait_snk);
		return 0;
	}

	return 0;
}

static unsigned int tc_state_attached_src_exit(int port)
{
	/*
	 * A port that is supplying VCONN shall cease to supply it within
	 * tVCONNOFF of exiting Attached.SRC, unless it is exiting as a
	 * result of a USB PD PR_Swap or is transitioning into the
	 * CTUnattached.SNK state
	 */
	if (IS_ENABLED(CONFIG_USBC_VCONN))
		set_vconn(port, 0);

	/*
	 * A port shall cease to supply VBUS within tVBUSOFF of exiting
	 * Attached.SRC.
	 */
	tc_src_power_off(port);

	return 0;
}

/**
 * Try.SRC
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rp on CC
 *   Set power role to SOURCE
 */
#ifdef CONFIG_USB_PD_TRY_SRC
static unsigned int tc_state_try_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_try_src_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_try_src_entry(int port)
{
	tc[port].state_id = TRY_SRC;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].cc_state = PD_CC_UNSET;
	tc[port].try_wait_debounce = get_time().val + PD_T_DRP_TRY;
	tc[port].timeout = get_time().val + PD_T_TRY_TIMEOUT;
	return 0;
}

static unsigned int tc_state_try_src_run(int port)
{
	int cc1;
	int cc2;
	int new_cc_state;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if ((cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_OPEN) ||
	     (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_RD))
		new_cc_state = PD_CC_UFP_ATTACHED;
	else
		new_cc_state = PD_CC_NONE;

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
	}

	/*
	 * The port shall transition to Attached.SRC when the SRC.Rd state is
	 * detected on exactly one of the CC1 or CC2 pins for at least
	 * tTryCCDebounce.
	 */
	if (get_time().val > tc[port].cc_debounce) {
		if (new_cc_state == PD_CC_UFP_ATTACHED)
			set_state(port, TC_OBJ(port), tc_state_attached_src);
	}

	/*
	 * The port shall transition to TryWait.SNK after tDRPTry and the
	 * SRC.Rd state has not been detected and VBUS is within vSafe0V,
	 * or after tTryTimeout and the SRC.Rd state has not been detected.
	 */
	if (new_cc_state == PD_CC_NONE) {
		if ((get_time().val > tc[port].try_wait_debounce &&
					!pd_is_vbus_present(port)) ||
					get_time().val > tc[port].timeout) {
			set_state(port, TC_OBJ(port), tc_state_try_wait_snk);
		}
	}

	return 0;
}

#endif

/**
 * TryWait.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
static unsigned int tc_state_try_wait_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_try_wait_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rd);
}

static unsigned int tc_state_try_wait_snk_entry(int port)
{
	tc[port].state_id = TRY_WAIT_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	tc[port].cc_state = PD_CC_UNSET;
	tc[port].try_wait_debounce = get_time().val + PD_T_CC_DEBOUNCE;
	return 0;
}

static unsigned int tc_state_try_wait_snk_run(int port)
{
	int cc1;
	int cc2;
	int new_cc_state;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	/* We only care about CCs being open */
	if (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_OPEN)
		new_cc_state = PD_CC_NONE;
	else
		new_cc_state = PD_CC_UNSET;

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = new_cc_state;
		tc[port].pd_debounce = get_time().val + PD_T_PD_DEBOUNCE;
	}

	/*
	 * The port shall transition to Unattached.SNK when the state of both
	 * of the CC1 and CC2 pins is SNK.Open for at least tPDDebounce.
	 */
	if ((get_time().val > tc[port].pd_debounce) &&
						(new_cc_state == PD_CC_NONE)) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		return 0;
	}

	/*
	 * The port shall transition to Attached.SNK after tCCDebounce if or
	 * when VBUS is detected.
	 */
	if ((get_time().val > tc[port].try_wait_debounce) ||
					pd_is_vbus_present(port)) {
		set_state(port, TC_OBJ(port), tc_state_attached_snk);
	}

	return 0;
}

/**
 * Super State CC_RD
 */
static unsigned int tc_state_cc_rd(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_cc_rd_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_cc_rd_entry(int port)
{
	/* Disable VCONN */
	if (IS_ENABLED(CONFIG_USBC_VCONN))
		set_vconn(port, 0);

	/*
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rd.
	 */
	tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
	tcpm_set_cc(port, TYPEC_CC_RD);

	/* Set power role to sink */
	tc_set_power_role(port, PD_ROLE_SINK);
	tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);

	return 0;
}

/**
 * Super State CC_RP
 */
static unsigned int tc_state_cc_rp(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_cc_rp_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_cc_rp_entry(int port)
{
	/* Disable VCONN */
	if (IS_ENABLED(CONFIG_USBC_VCONN))
		set_vconn(port, 0);

	/* Set power role to source */
	tc_set_power_role(port, PD_ROLE_SOURCE);

	/*
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rp.
	 */
	tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
	tcpm_set_cc(port, TYPEC_CC_RP);

	return 0;
}

/**
 * Super State CC_OPEN
 */
static unsigned int tc_state_cc_open(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_cc_open_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_cc_open_entry(int port)
{
	/* Disable VCONN */
	if (IS_ENABLED(CONFIG_USBC_VCONN))
		set_vconn(port, 0);

	/* Remove terminations from CC */
	tcpm_set_cc(port, TYPEC_CC_OPEN);

	return 0;
}

static unsigned int do_nothing(int port)
{
	return 0;
}

static unsigned int get_super_state(int port)
{
	return RUN_SUPER;
}
#endif /* __CROS_EC_USB_TC_DRP_ACC_TRYSRC_H */
