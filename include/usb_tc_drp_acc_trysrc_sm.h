/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "usb_emsg.h"

/* USB Type-C DRP with Accessory and Try.SRC module */

#ifndef __CROS_EC_USB_TC_DRP_ACC_TRYSRC_H
#define __CROS_EC_USB_TC_DRP_ACC_TRYSRC_H

#define SET_FLAG(port, flag) tc[port].flags  |=  (flag)
#define CLR_FLAG(port, flag) tc[port].flags  &= ~(flag)
#define CHK_FLAG(port, flag) (tc[port].flags &   (flag))

/* Type-C Layer Flags */
#define TC_FLAGS_CTVPD_DETECTED           BIT(0)
#define TC_FLAGS_REQUEST_VC_SWAP_ON       BIT(1)
#define TC_FLAGS_REQUEST_VC_SWAP_OFF      BIT(2)
#define TC_FLAGS_REJECT_VCONN_SWAP        BIT(3)
#define TC_FLAGS_VCONN_ON                 BIT(4)
#define TC_FLAGS_REQUEST_PR_SWAP          BIT(5)
#define TC_FLAGS_REQUEST_DR_SWAP          BIT(6)
#define TC_FLAGS_PS_SWAP_IN_PROGRESS      BIT(7)
#define TC_FLAGS_POWER_OFF_SNK            BIT(8)
#define TC_FLAGS_PARTNER_EXTPOWER         BIT(9)
#define TC_FLAGS_PARTNER_USB_COMM         BIT(10)
#define TC_FLAGS_PARTNER_DR_DATA          BIT(11)
#define TC_FLAGS_TS_DTS_PARTNER           BIT(12)
#define TC_FLAGS_PARTNER_DR_POWER         BIT(13)
#define TC_FLAGS_PREVIOUS_PD_CONN         BIT(14) 
#define TC_FLAGS_LPM_REQUESTED            BIT(15)
#define TC_FLAGS_LPM_TRANSITION           BIT(16)
#define TC_FLAGS_LPM_ENGAGED              BIT(17)
#define TC_FLAGS_VBUS_NEVER_LOW           BIT(18)
#define TC_FLAGS_HARD_RESET               BIT(19)

#undef PD_DEFAULT_STATE
/* Port default state at startup */
#define PD_DEFAULT_STATE(port) tc_state_unattached_snk

#define TC_OBJ(port)   (SM_OBJ(tc[port]))
#define TC_TEST_OBJ(port) (SM_OBJ(tc[(port)].obj))

/* Type C supply voltage (mV) */
#define TYPE_C_VOLTAGE	5000 /* mV */

/* Type C default sink current (mA) */
#define TYPE_C_CURRENT  500 /* mA */

enum ps_reset_sequence {
	PS_STATE0,
	PS_STATE1,
	PS_STATE2
};

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
/* Tracker for which task is waiting on sysjump prep to finish */
static volatile task_id_t sysjump_task_waiting = TASK_ID_INVALID;
#endif


static struct type_c {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	/* state id */
	enum typec_state_id state_id;
	/* current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* enable power delivery state machines */
	uint8_t pd_enable;
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
	/* Power supply reset sequence during a hard reset */
	enum ps_reset_sequence ps_reset_state;
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
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
        /* Time to enter low power mode */
        uint64_t low_power_time;
        /* Tasks to notify after TCPC has been reset */
        int tasks_waiting_on_reset;
        /* Tasks preventing TCPC from entering low power mode */
        int tasks_preventing_lpm;
#endif

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
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
static void handle_device_access(int port);
static int pd_device_in_low_power(int port);
static void pd_wait_for_wakeup(int port);
#endif /* CONFIG_USB_PD_TCPC_LOW_POWER */
static void set_usb_mux_with_current_data_role(int port);


#if defined(CONFIG_CHARGE_MANAGER)
static typec_current_t get_typec_current_limit(int polarity, int cc1, int cc2);
#endif

#ifdef CONFIG_USB_PD_TRY_SRC
/* Enable variable for Try.SRC states */
static uint8_t pd_try_src_enable;
static void pd_update_try_source(void);
#endif

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

#if defined(CONFIG_USB_PRL_SM) && defined(CONFIG_USB_PE_SM)
static unsigned int tc_state_ct_unattached_snk(int port, enum signal sig);
static unsigned int tc_state_ct_unattached_snk_entry(int port);
static unsigned int tc_state_ct_unattached_snk_run(int port);

static unsigned int tc_state_ct_attached_snk(int port, enum signal sig);
static unsigned int tc_state_ct_attached_snk_entry(int port);
static unsigned int tc_state_ct_attached_snk_run(int port);
static unsigned int tc_state_ct_attached_snk_exit(int port);
#endif

/* Super States */

static unsigned int tc_state_cc_rd(int port, enum signal sig);
static unsigned int tc_state_cc_rd_entry(int port);

static unsigned int tc_state_cc_rp(int port, enum signal sig);
static unsigned int tc_state_cc_rp_entry(int port);

static unsigned int tc_state_cc_open(int port, enum signal sig);
static unsigned int tc_state_cc_open_entry(int port);

static unsigned int do_nothing(int port);
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

#ifdef CONFIG_USB_PD_TRY_SRC
static const state_sig tc_state_try_src_sig[] = {
	tc_state_try_src_entry,
	tc_state_try_src_run,
	do_nothing,
	get_super_state
};
#endif

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

#if defined(CONFIG_USB_PRL_SM) && defined(CONFIG_USB_PE_SM)
static const state_sig tc_state_ct_unattached_snk_sig[] = {
	tc_state_ct_unattached_snk_entry,
	tc_state_ct_unattached_snk_run,
	do_nothing,
	get_super_state
};

static const state_sig tc_state_ct_attached_snk_sig[] = {
	tc_state_ct_attached_snk_entry,
	tc_state_ct_attached_snk_run,
	tc_state_ct_attached_snk_exit,
	get_super_state
};
#endif

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

#if !defined(CONFIG_USB_PRL_SM) || !defined(CONFIG_USB_PE_SM)
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

void pd_set_dual_role(int port, enum pd_dual_role_states state)
{
	drp_state[port] = state;

#ifdef CONFIG_USB_PD_TRY_SRC
	pd_update_try_source();
#endif
}

int pd_get_partner_data_swap_capable(int port)
{
	/* return data swap capable status of port partner */
	return CHK_FLAG(port, TC_FLAGS_PARTNER_DR_DATA);
}

int pd_comm_is_enabled(int port)
{
	return tc[port].pd_enable;
}

void pd_send_vdm(int port, uint32_t vid, int cmd, const uint32_t *data,
			int count)
{
	pe_send_vdm(port, vid, cmd, data, count);
}

#ifdef CONFIG_COMMON_RUNTIME
void pd_set_suspend(int port, int enable)
{
	if (enable)
		set_state(port, TC_OBJ(port), tc_state_disabled);
	else
		set_state(port, TC_OBJ(port), PD_DEFAULT_STATE(port));
}

int pd_is_port_enabled(int port)
{
	return !(tc[port].obj.task_state == tc_state_disabled);
}

int pd_fetch_acc_log_entry(int port)
{
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	pd_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_GET_LOG, NULL, 0);
#endif
	return EC_RES_SUCCESS;
}
#endif

void pd_request_data_swap(int port)
{
	/*
	 * Must be in Attached.SRC or Attached.SNK when this function
	 * is called
	 */
        if (tc[port].state_id == ATTACHED_SRC ||
					tc[port].state_id == ATTACHED_SNK) {
		SET_FLAG(port, TC_FLAGS_REQUEST_DR_SWAP);
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
	}
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
void pd_prepare_sysjump(void)
{
        int i;

        /* Exit modes before sysjump so we can cleanly enter again later */
        for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
                /*
                 * We can't be in an alternate mode if PD comm is disabled, so
                 * no need to send the event
                 */
                if (!pd_comm_is_enabled(i))
                        continue;

                sysjump_task_waiting = task_get_current();
                task_set_event(PD_PORT_TO_TASK_ID(i), PD_EVENT_SYSJUMP, 0);
                task_wait_event_mask(TASK_EVENT_SYSJUMP_READY, -1);
                sysjump_task_waiting = TASK_ID_INVALID;
        }
}
#endif

int pd_get_power_role(int port)
{
	return tc[port].power_role;
}

void pd_update_contract(int port)
{
#ifdef CONFIG_USB_PE_SM
	/* Must be in Attached.SRC when this function is called */
	if (tc[port].state_id == ATTACHED_SRC)
		pe_dpm_request(port, DPM_REQUEST_SRC_CAP_CHANGE);
#endif
}

void pd_request_source_voltage(int port, int mv)
{
#ifdef CONFIG_USB_PE_SM
	pd_set_max_voltage(mv);

	/* Must be in Attached.SNK when this function is called */
	if (tc[port].state_id == ATTACHED_SNK)
		pe_dpm_request(port, DPM_REQUEST_NEW_POWER_LEVEL);
	else
		SET_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);

	task_wake(PD_PORT_TO_TASK_ID(port));
#endif
}

void pd_set_external_voltage_limit(int port, int mv)
{
#ifdef CONFIG_USB_PE_SM
	pd_set_max_voltage(mv);

	/* Must be in Attached.SNK when this function is called */
	if (tc[port].state_id == ATTACHED_SNK)
		pe_dpm_request(port, DPM_REQUEST_NEW_POWER_LEVEL);

	task_wake(PD_PORT_TO_TASK_ID(port));
#endif
}

int pd_get_polarity(int port)
{
	return tc[port].polarity;
}

int pd_get_role(int port)
{
	return tc[port].data_role;
}

void pd_set_new_power_request(int port)
{
#ifdef CONFIG_USB_PE_SM
	pe_dpm_request(port, DPM_REQUEST_NEW_POWER_LEVEL);
#endif
}

/*
 * Return true if partner port is a DTS or TS capable of entering debug
 * mode (eg. is presenting Rp/Rp or Rd/Rd).
 */
int pd_ts_dts_plugged(int port)
{
        return CHK_FLAG(port, TC_FLAGS_TS_DTS_PARTNER);
}

/* Return true if partner port is known to be PD capable. */
int pd_capable(int port)
{
        return CHK_FLAG(port, TC_FLAGS_PREVIOUS_PD_CONN);
}

/*
 * Return true if partner port is capable of communication over USB data
 * lines.
 */
int pd_get_partner_usb_comm_capable(int port)
{
        return CHK_FLAG(port, TC_FLAGS_PARTNER_USB_COMM);
}

void pd_vbus_low(int port)
{
	CLR_FLAG(port, TC_FLAGS_VBUS_NEVER_LOW);
}

int pd_is_vbus_present(int port)
{
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	return tcpm_get_vbus_level(port);
#else
	return pd_snk_is_vbus_provided(port);
#endif
}

void pd_request_power_swap(int port)
{
	/*
	 * Must be in Attached.SRC or Attached.SNK when this function
	 * is called
	 */
        if (tc[port].state_id == ATTACHED_SRC ||
					tc[port].state_id == ATTACHED_SNK) {
		SET_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
	}
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

enum pd_dual_role_states pd_get_dual_role(int port)
{
	return drp_state[port];
}

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
/*
 * This can be called from any task. If we are in the PD task, we can handle
 * immediately. Otherwise, we need to notify the PD task via event.
 */
void pd_device_accessed(int port)
{
        if (port == TASK_ID_TO_PD_PORT(task_get_current())) {
                /* Ignore any access to device while it is waking up */
                if (CHK_FLAG(port, TC_FLAGS_LPM_TRANSITION))
                        return;

                handle_device_access(port);
        } else {
                task_set_event(PD_PORT_TO_TASK_ID(port),
                               PD_EVENT_DEVICE_ACCESSED, 0);
        }
}

void pd_prevent_low_power_mode(int port, int prevent)
{
        const int current_task_mask = (1 << task_get_current());

        if (prevent)
                atomic_or(&tc[port].tasks_preventing_lpm, current_task_mask);
        else
                atomic_clear(&tc[port].tasks_preventing_lpm, current_task_mask);
}

void pd_wait_exit_low_power(int port)
{
        if (pd_device_in_low_power(port))
                pd_wait_for_wakeup(port);
}
#endif /* CONFIG_USB_PD_TCPC_LOW_POWER */

#ifdef CONFIG_USBC_VCONN_SWAP
void pd_request_vconn_swap_off(int port)
{
	if (tc[port].state_id == ATTACHED_SRC ||
			tc[port].state_id == ATTACHED_SNK) {
		SET_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_OFF);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}

void pd_request_vconn_swap_on(int port)
{
	if (tc[port].state_id == ATTACHED_SRC ||
			tc[port].state_id == ATTACHED_SNK) {
		SET_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_ON);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}
#endif

#ifdef CONFIG_USBC_VCONN
int tc_is_vconn_src(int port)
{
	if (tc[port].state_id == ATTACHED_SRC ||
				tc[port].state_id == ATTACHED_SNK)
		return CHK_FLAG(port, TC_FLAGS_VCONN_ON);
	else
		return -1;
}
#endif

void tc_start_error_recovery(int port)
{
        set_state(port, TC_OBJ(port), tc_state_error_recovery);
}

void tc_partner_dr_power(int port, int en)
{
	if (en)
		SET_FLAG(port, TC_FLAGS_PARTNER_DR_POWER);
	else
		CLR_FLAG(port, TC_FLAGS_PARTNER_DR_POWER);
}

void tc_partner_extpower(int port, int en)
{
	if (en)
		SET_FLAG(port, TC_FLAGS_PARTNER_EXTPOWER);
	else
		CLR_FLAG(port, TC_FLAGS_PARTNER_EXTPOWER);

}

void tc_partner_usb_comm(int port, int en)
{
	if (en)
		SET_FLAG(port, TC_FLAGS_PARTNER_USB_COMM);
	else
		CLR_FLAG(port, TC_FLAGS_PARTNER_USB_COMM);

}

void tc_partner_dr_data(int port, int en)
{
	if (en)
		SET_FLAG(port, TC_FLAGS_PARTNER_DR_DATA);
	else
		CLR_FLAG(port, TC_FLAGS_PARTNER_DR_DATA);

}

void tc_pd_connection(int port, int en)
{
	if (en)
		SET_FLAG(port, TC_FLAGS_PREVIOUS_PD_CONN);
	else
		CLR_FLAG(port, TC_FLAGS_PREVIOUS_PD_CONN);
}

void tc_ctvpd_detected(int port)
{
	SET_FLAG(port, TC_FLAGS_CTVPD_DETECTED);
}

void tc_vconn_on(int port)
{
#ifdef CONFIG_USBC_VCONN
	set_vconn(port, 1);
#endif
}

int tc_check_vconn_swap(int port)
{
#ifdef CONFIG_USBC_VCONN
	if (CHK_FLAG(port, TC_FLAGS_REJECT_VCONN_SWAP))
		return 0;

	return pd_check_vconn_swap(port);
#else
	return 0;
#endif
}

void tc_pr_swap_complete(int port)
{
	CLR_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS);
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
}

void tc_power_off_snk(int port)
{
	if (tc[port].state_id == ATTACHED_SNK) {
		SET_FLAG(port, TC_FLAGS_POWER_OFF_SNK);
		/* Stop drawing power */
		pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
						CHARGE_CEIL_NONE);
#endif
	}
}

int tc_src_power_on(int port)
{
	if (tc[port].state_id == ATTACHED_SRC)
		return pd_set_power_supply_ready(port);
	else
		return 0;
}

void tc_src_power_off(int port)
{
	if (tc[port].state_id == ATTACHED_SRC) {
		/* Remove VBUS */
		pd_power_supply_reset(port);
		pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
						CHARGE_CEIL_NONE);
#endif
	}
}

void tc_prs_src_snk_assert_rd(int port)
{
	/* Must be in Attached.SRC when this function is called */
	if (tc[port].state_id == ATTACHED_SRC) {
		/* Transition to Attached.SNK to assert Rd */
		SET_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
	}
}

void tc_prs_snk_src_assert_rp(int port)
{
	/* Must be in Attached.SNK when this function is called */
	if (tc[port].state_id == ATTACHED_SNK) {
		/* Transition to Attached.SRC to assert Rp */
		SET_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);
		task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
	}
}

void tc_hard_reset(int port)
{
	SET_FLAG(port, TC_FLAGS_HARD_RESET);
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
}

void tc_state_init(int port)
{
	int res = 0;
	sm_state this_state;

	res = tc_restart_tcpc(port);

	CPRINTS("TCPC p%d init %s", port, res ? "failed" : "ready");
	this_state = res ? tc_state_disabled : PD_DEFAULT_STATE(port);

	init_state(port, TC_OBJ(port), this_state);

#ifdef CONFIG_USBC_SS_MUX
	/* Initialize USB mux to its default state */
	usb_mux_init(port);
#endif

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	tcpm_select_rp_value(port, CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT);
#else
	tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
#endif

#ifdef CONFIG_CHARGE_MANAGER
	/* Initialize PD and type-C supplier current limits to 0 */
	pd_set_input_current_limit(port, 0, 0);
	typec_set_input_current_limit(port, 0, 0);
	charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif

	/* Disable pd state machines */
	tc[port].flags = 0;
	tc[port].pd_enable = 0;
	tc[port].evt_timeout = 5*MSEC;
	tc[port].ps_reset_state = PS_STATE0;
}

/*
 * Private Functions
 */

#ifdef CONFIG_USB_PD_TCPC_LOW_POWER

/* 10 ms is enough time for any TCPC transaction to complete. */
#define PD_LPM_DEBOUNCE_US (10 * MSEC)

/* This is only called from the PD tasks that owns the port. */
static void handle_device_access(int port)
{
        /* This should only be called from the PD task */
        assert(port == TASK_ID_TO_PD_PORT(task_get_current()));

        tc[port].low_power_time = get_time().val + PD_LPM_DEBOUNCE_US;
        if (CHK_FLAG(port, TC_FLAGS_LPM_ENGAGED)) {
                CPRINTS("TCPC p%d Exit Low Power Mode", port);
                CLR_FLAG(port, TC_FLAGS_LPM_ENGAGED |
                                    TC_FLAGS_LPM_REQUESTED);
                /*
                 * Wake to ensure we make another pass through the main task
                 * loop after clearing the flags.
                 */
                task_wake(PD_PORT_TO_TASK_ID(port));
        }
}

static int pd_device_in_low_power(int port)
{
        /*
         * If we are actively waking the device up in the PD task, do not
         * let TCPC operation wait or retry because we are in low power mode.
         */
        if (port == TASK_ID_TO_PD_PORT(task_get_current()) &&
            (CHK_FLAG(port, TC_FLAGS_LPM_TRANSITION)))
                return 0;

        return CHK_FLAG(port, TC_FLAGS_LPM_ENGAGED);
}

static int reset_device_and_notify(int port)
{
        int rv;
        int task, waiting_tasks;

        /* This should only be called from the PD task */
        assert(port == TASK_ID_TO_PD_PORT(task_get_current()));

        SET_FLAG(port, TC_FLAGS_LPM_TRANSITION);
        rv = tcpm_init(port);
        CLR_FLAG(port, TC_FLAGS_LPM_TRANSITION);

        if (rv == EC_SUCCESS)
                CPRINTS("TCPC p%d init ready", port);
        else
                CPRINTS("TCPC p%d init failed!", port);

        /*
         * Before getting the other tasks that are waiting, clear the reset
         * event from this PD task to prevent multiple reset/init events
         * occurring.
         *
         * The double reset event happens when the higher priority PD interrupt
         * task gets an interrupt during the above tcpm_init function. When that
         * occurs, the higher priority task waits correctly for us to finish
         * waking the TCPC, but it has also set PD_EVENT_TCPC_RESET again, which
         * would result in a second, unnecessary init.
         */
        atomic_clear(task_get_event_bitmap(task_get_current()),
                     PD_EVENT_TCPC_RESET);

        waiting_tasks = atomic_read_clear(&tc[port].tasks_waiting_on_reset);

        /*
         * Now that we are done waking up the device, handle device access
         * manually because we ignored it while waking up device.
         */
        handle_device_access(port);

        /* Clear SW LPM state; the state machine will set it again if needed */
        CLR_FLAG(port, TC_FLAGS_LPM_REQUESTED);

        /* Wake up all waiting tasks. */
        while (waiting_tasks) {
                task = __fls(waiting_tasks);
                waiting_tasks &= ~(1 << task);
                task_set_event(task, TASK_EVENT_PD_AWAKE, 0);
        }

        return rv;
}

static void pd_wait_for_wakeup(int port)
{
        if (port == TASK_ID_TO_PD_PORT(task_get_current())) {
                /* If we are in the PD task, we can directly reset */
                reset_device_and_notify(port);
        } else {
                /* Otherwise, we need to wait for the TCPC reset to complete */
                atomic_or(&tc[port].tasks_waiting_on_reset,
                          1 << task_get_current());
                /*
                 * NOTE: We could be sending the PD task the reset event while
                 * it is already processing the reset event. If that occurs,
                 * then we will reset the TCPC multiple times, which is
                 * undesirable but most likely benign. Empirically, this doesn't
                 * happen much, but it if starts occurring, we can add a guard
                 * to prevent/reduce it.
                 */
                task_set_event(PD_PORT_TO_TASK_ID(port),
                               PD_EVENT_TCPC_RESET, 0);
                task_wait_event_mask(TASK_EVENT_PD_AWAKE, -1);
        }
}

/* This is only called from the PD tasks that owns the port. */
static void exit_low_power_mode(int port)
{
	if (CHK_FLAG(port, TC_FLAGS_LPM_ENGAGED))
		reset_device_and_notify(port);
	else
		CLR_FLAG(port,TC_FLAGS_LPM_REQUESTED);
}
#endif /* !CONFIG_USB_PD_TCPC_LOW_POWER */

#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)

/* This must only be called from the PD task */
static void pd_update_dual_role_config(int port)
{
	/*
	 * Change to sink if port is currently a source AND (new DRP
	 * state is force sink OR new DRP state is either toggle off
	 * or debug accessory toggle only and we are in the source
	 * disconnected state).
	 */
	if (tc[port].power_role == PD_ROLE_SOURCE &&
	    ((drp_state[port] == PD_DRP_FORCE_SINK && !pd_ts_dts_plugged(port))
	     || (drp_state[port] == PD_DRP_TOGGLE_OFF
	     	&& tc[port].obj.task_state == tc_state_unattached_src))) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
	}

	/*
	 * Change to source if port is currently a sink and the
	 * new DRP state is force source.
	 */
	else if (tc[port].power_role == PD_ROLE_SINK &&
	    drp_state[port] == PD_DRP_FORCE_SOURCE) {
	    set_state(port, TC_OBJ(port), tc_state_unattached_src);
	}
}


#ifdef CONFIG_POWER_COMMON
static void handle_new_power_state(int port)
{
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF))
		/* The SoC will negotiated DP mode again when it boots up */
		pe_exit_dp_mode(port);
#endif
	/* Ensure mux is set properly after chipset transition */
	set_usb_mux_with_current_data_role(port);
}
#endif /* CONFIG_POWER_COMMON */


#ifdef HAS_TASK_HOSTCMD
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

static const enum pd_dual_role_states dual_role_map[USB_PD_CTRL_ROLE_COUNT] = {
        [USB_PD_CTRL_ROLE_TOGGLE_ON]    = PD_DRP_TOGGLE_ON,
        [USB_PD_CTRL_ROLE_TOGGLE_OFF]   = PD_DRP_TOGGLE_OFF,
        [USB_PD_CTRL_ROLE_FORCE_SINK]   = PD_DRP_FORCE_SINK,
        [USB_PD_CTRL_ROLE_FORCE_SOURCE] = PD_DRP_FORCE_SOURCE,
        [USB_PD_CTRL_ROLE_FREEZE]       = PD_DRP_FREEZE,
};

#ifdef CONFIG_USBC_SS_MUX
static const enum typec_mux typec_mux_map[USB_PD_CTRL_MUX_COUNT] = {
        [USB_PD_CTRL_MUX_NONE] = TYPEC_MUX_NONE,
        [USB_PD_CTRL_MUX_USB]  = TYPEC_MUX_USB,
        [USB_PD_CTRL_MUX_AUTO] = TYPEC_MUX_DP,
        [USB_PD_CTRL_MUX_DP]   = TYPEC_MUX_DP,
        [USB_PD_CTRL_MUX_DOCK] = TYPEC_MUX_DOCK,
};
#endif

static int hc_usb_pd_control(struct host_cmd_handler_args *args)
{
	const struct ec_params_usb_pd_control *p = args->params;
	struct ec_response_usb_pd_control_v1 *r_v1 = args->response;
	struct ec_response_usb_pd_control *r = args->response;

	if (p->port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	if (p->role >= USB_PD_CTRL_ROLE_COUNT ||
				p->mux >= USB_PD_CTRL_MUX_COUNT)
		return EC_RES_INVALID_PARAM;

	if (p->role != USB_PD_CTRL_ROLE_NO_CHANGE)
		pd_set_dual_role(p->port, dual_role_map[p->role]);

#ifdef CONFIG_USBC_SS_MUX
	if (p->mux != USB_PD_CTRL_MUX_NO_CHANGE)
		usb_mux_set(p->port, typec_mux_map[p->mux],
			    typec_mux_map[p->mux] == TYPEC_MUX_NONE ?
			    USB_SWITCH_DISCONNECT :
			    USB_SWITCH_CONNECT,
			    pd_get_polarity(p->port));
#endif /* CONFIG_USBC_SS_MUX */

	if (p->swap == USB_PD_CTRL_SWAP_DATA)
		pd_request_data_swap(p->port);
	else if (p->swap == USB_PD_CTRL_SWAP_POWER)
		pd_request_power_swap(p->port);
#ifdef CONFIG_USBC_VCONN_SWAP
	else if (p->swap == USB_PD_CTRL_SWAP_VCONN)
		pe_dpm_request(p->port, DPM_REQUEST_VCONN_SWAP);
#endif

	if (args->version == 0) {
		r->enabled = pd_comm_is_enabled(p->port);
		r->role = tc[p->port].power_role;
		r->polarity = tc[p->port].polarity;
		r->state = tc[p->port].state_id;
		args->response_size = sizeof(*r);
	} else {
		r_v1->enabled =
			(pd_comm_is_enabled(p->port) ?
				PD_CTRL_RESP_ENABLED_COMMS : 0) |
			(pd_is_connected(p->port) ?
				PD_CTRL_RESP_ENABLED_CONNECTED : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_PREVIOUS_PD_CONN) ?
				PD_CTRL_RESP_ENABLED_PD_CAPABLE : 0);
		r_v1->role =
			(tc[p->port].power_role ? PD_CTRL_RESP_ROLE_POWER : 0) |
			(tc[p->port].data_role ? PD_CTRL_RESP_ROLE_DATA : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_VCONN_ON) ?
				PD_CTRL_RESP_ROLE_VCONN : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_PARTNER_DR_POWER) ?
				PD_CTRL_RESP_ROLE_DR_POWER : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_PARTNER_DR_DATA) ?
				PD_CTRL_RESP_ROLE_DR_DATA : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_PARTNER_USB_COMM) ?
				PD_CTRL_RESP_ROLE_USB_COMM : 0) |
			(CHK_FLAG(p->port, TC_FLAGS_PARTNER_EXTPOWER) ?
				PD_CTRL_RESP_ROLE_EXT_POWERED : 0);
		r_v1->polarity = tc[p->port].polarity;
		strzcpy(r_v1->state,
			tc_state_names[tc[p->port].state_id],
			sizeof(r_v1->state));
		args->response_size = sizeof(*r_v1);
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_CONTROL,
                     hc_usb_pd_control,
                     EC_VER_MASK(0) | EC_VER_MASK(1));

static int hc_remote_flash(struct host_cmd_handler_args *args)
{
        const struct ec_params_usb_pd_fw_update *p = args->params;
        int port = p->port;
	int rv = EC_RES_SUCCESS;
        const uint32_t *data = &(p->size) + 1;
        int i, size;

        if (port >= CONFIG_USB_PD_PORT_COUNT)
                return EC_RES_INVALID_PARAM;

        if (p->size + sizeof(*p) > args->params_size)
                return EC_RES_INVALID_PARAM;

#if defined(CONFIG_BATTERY_PRESENT_CUSTOM) ||   \
        defined(CONFIG_BATTERY_PRESENT_GPIO)
        /*
         * Do not allow PD firmware update if no battery and this port
         * is sinking power, because we will lose power.
         */
        if (battery_is_present() != BP_YES &&
            charge_manager_get_active_charge_port() == port)
                return EC_RES_UNAVAILABLE;
#endif

        switch (p->cmd) {
        case USB_PD_FW_REBOOT:
                pe_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_REBOOT, NULL, 0);
                /*
                 * Return immediately to free pending i2c bus.  Host needs to
                 * manage this delay.
                 */
                return EC_RES_SUCCESS;

        case USB_PD_FW_FLASH_ERASE:
                pe_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_FLASH_ERASE, NULL, 0);
                /*
                 * Return immediately.  Host needs to manage delays here which
                 * can be as long as 1.2 seconds on 64KB RW flash.
                 */
                return EC_RES_SUCCESS;

        case USB_PD_FW_ERASE_SIG:
                pe_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_ERASE_SIG, NULL, 0);
                break;

	case USB_PD_FW_FLASH_WRITE:
                /* Data size must be a multiple of 4 */
                if (!p->size || p->size % 4)
                        return EC_RES_INVALID_PARAM;

                size = p->size / 4;
                for (i = 0; i < size; i += VDO_MAX_SIZE - 1) {
                        pe_send_vdm(port, USB_VID_GOOGLE, VDO_CMD_FLASH_WRITE,
                                    data + i, MIN(size - i, VDO_MAX_SIZE - 1));
                }
                return EC_RES_SUCCESS;

        default:
                return EC_RES_INVALID_PARAM;
                break;
        }

        return rv;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_FW_UPDATE,
                     hc_remote_flash,
                     EC_VER_MASK(0));

static int hc_remote_rw_hash_entry(struct host_cmd_handler_args *args)
{
        int i, idx = 0, found = 0;
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

        if (r->dev_id) {
                memcpy(r->dev_rw_hash, tc[*port].dev_rw_hash,
                       PD_RW_HASH_SIZE);
        }

        r->current_image = tc[*port].current_image;

        args->response_size = sizeof(*r);
        return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_USB_PD_DEV_INFO,
                     hc_remote_pd_dev_info,
                     EC_VER_MASK(0));
#ifndef CONFIG_USB_PD_TCPC
#ifdef CONFIG_EC_CMD_PD_CHIP_INFO
static int hc_remote_pd_chip_info(struct host_cmd_handler_args *args)
{
        const struct ec_params_pd_chip_info *p = args->params;
        struct ec_response_pd_chip_info_v1 *info;

        if (p->port >= CONFIG_USB_PD_PORT_COUNT)
                return EC_RES_INVALID_PARAM;

        if (tcpm_get_chip_info(p->port, p->renew, &info))
                return EC_RES_ERROR;

        /*
         * Take advantage of the fact that v0 and v1 structs have the
         * same layout for v0 data. (v1 just appends data)
         */
        args->response_size =
                args->version ? sizeof(struct ec_response_pd_chip_info_v1)
                              : sizeof(struct ec_response_pd_chip_info);

        memcpy(args->response, info, args->response_size);

        return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_PD_CHIP_INFO,
                     hc_remote_pd_chip_info,
                     EC_VER_MASK(0) | EC_VER_MASK(1));
#endif /* CONFIG_EC_CMD_PD_CHIP_INFO */
#endif /* !CONFIG_USB_PD_TCPC */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static int hc_remote_pd_set_amode(struct host_cmd_handler_args *args)
{
        const struct ec_params_usb_pd_set_mode_request *p = args->params;

        if ((p->port >= CONFIG_USB_PD_PORT_COUNT) || (!p->svid) || (!p->opos))
                return EC_RES_INVALID_PARAM;

        switch (p->cmd) {
        case PD_EXIT_MODE:
                if (pd_dfp_exit_mode(p->port, p->svid, p->opos))
                        pd_send_vdm(p->port, p->svid,
                                    CMD_EXIT_MODE | VDO_OPOS(p->opos), NULL, 0);
                else {
                        CPRINTF("Failed exit mode\n");
                        return EC_RES_ERROR;
                }
                break;
        case PD_ENTER_MODE:
                if (pd_dfp_enter_mode(p->port, p->svid, p->opos))
                        pd_send_vdm(p->port, p->svid, CMD_ENTER_MODE |
                                    VDO_OPOS(p->opos), NULL, 0);
                break;
        default:
                return EC_RES_INVALID_PARAM;
        }
        return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_SET_AMODE,
                     hc_remote_pd_set_amode,
                     EC_VER_MASK(0));
#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
#endif /* HAS_TASK_HOSTCMD */

#if defined(CONFIG_USB_PD_ALT_MODE) && !defined(CONFIG_USB_PD_ALT_MODE_DFP)
void pd_send_hpd(int port, enum hpd_event hpd)
{
        uint32_t data[1];
        int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);
        if (!opos)
                return;

        data[0] = VDO_DP_STATUS((hpd == hpd_irq),  /* IRQ_HPD */
                                (hpd != hpd_low),  /* HPD_HI|LOW */
                                0,                    /* request exit DP */
                                0,                    /* request exit USB */
                                0,                    /* MF pref */
                                1,                    /* enabled */
                                0,                    /* power low */
                                0x2);
        pd_send_vdm(port, USB_SID_DISPLAYPORT,
                    VDO_OPOS(opos) | CMD_ATTENTION, data, 1);
}
#endif

#endif /* */

static void set_usb_mux_with_current_data_role(int port)
{
#ifdef CONFIG_USBC_SS_MUX
        /*
         * If the SoC is down, then we disconnect the MUX to save power since
         * no one cares about the data lines.
         */
#ifdef CONFIG_POWER_COMMON
        if (chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF)) {
                usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
                            tc[port].polarity);
                return;
        }
#endif /* CONFIG_POWER_COMMON */

#ifdef CONFIG_USBC_SS_MUX_DFP_ONLY
        /*
         * Need to connect SS mux for if new data role is DFP.
         * If new data role is UFP, then disconnect the SS mux.
         */
        if (tc[port].data_role == PD_ROLE_DFP)
                usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
                            tc[port].polarity);
        else
                usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
                            tc[port].polarity);
#else
        usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
                    tc[port].polarity);
#endif /* CONFIG_USBC_SS_MUX_DFP_ONLY */
#endif /* CONFIG_USBC_SS_MUX */
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
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	pd_execute_data_swap(port, role);
#endif
	set_usb_mux_with_current_data_role(port);

	/* Notify TCPC of role update */
	tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);
}

static void tc_event_check(int port, int evt)
{
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	if (evt & PD_EXIT_LOW_POWER_EVENT_MASK)
		exit_low_power_mode(port);
	if (evt & PD_EVENT_DEVICE_ACCESSED)
		handle_device_access(port);
#endif

#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_POWER_COMMON
	if (evt & PD_EVENT_POWER_STATE_CHANGE)
		handle_new_power_state(port);
#endif

#ifdef CONFIG_USB_PD_DUAL_ROLE
	if (evt & PD_EVENT_UPDATE_DUAL_ROLE)
		pd_update_dual_role_config(port);
#endif
#endif
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

#ifdef CONFIG_USB_PD_TRY_SRC
	pd_update_try_source();
#endif
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
/**
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
		SET_FLAG(port, TC_FLAGS_VCONN_ON);
	else
		CLR_FLAG(port, TC_FLAGS_VCONN_ON);
	
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


/* High-priority interrupt tasks implementations */
#if     defined(HAS_TASK_PD_INT_C0) || defined(HAS_TASK_PD_INT_C1) || \
        defined(HAS_TASK_PD_INT_C2)

/* Used to conditionally compile code in main pd task.  */
#define HAS_DEFFERED_INTERRUPT_HANDLER

/* Events for pd_interrupt_handler_task */
#define PD_PROCESS_INTERRUPT  (1<<0)

static uint8_t pd_int_task_id[CONFIG_USB_PD_PORT_COUNT];

void schedule_deferred_pd_interrupt(const int port)
{
	task_set_event(pd_int_task_id[port], PD_PROCESS_INTERRUPT, 0);
}

/**
* Main task entry point that handles PD interrupts for a single port
*
* @param p The PD port number for which to handle interrupts (pointer is
* reinterpreted as an integer directly).
*/
void pd_interrupt_handler_task(void *p)
{
	const int port = (int) p;
	const int port_mask = (PD_STATUS_TCPC_ALERT_0 << port);

	ASSERT(port >= 0 && port < CONFIG_USB_PD_PORT_COUNT);

	pd_int_task_id[port] = task_get_current();

	while (1) {
		const int evt = task_wait_event(-1);

		if (evt & PD_PROCESS_INTERRUPT) {
			/*
			 * While the interrupt signal is asserted; we have more
			 * work to do. This effectively makes the interrupt a
			 * level-interrupt instead of an edge-interrupt without
			 * having to enable/disable a real level-interrupt in
			 * multiple locations.
			 *
			 * Also, if the port is disabled do not process
			 * interrupts. Upon existing suspend, we schedule a
			 * PD_PROCESS_INTERRUPT to check if we missed anything.
			 */
			while ((tcpc_get_alert_status() & port_mask) &&
					pd_is_port_enabled(port))
				tcpc_alert(port);
		}
	}
}
#endif /* HAS_TASK_PD_INT_C0 || HAS_TASK_PD_INT_C1 || HAS_TASK_PD_INT_C2 */

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
 * TYPE-C State Implementations
 */

/**
 * Disabled
 *
 * Super State Entry Actions:
 *  Remove the terminations from CC
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
#ifndef CONFIG_USB_PD_TCPC
	if (tc_restart_tcpc(port) != 0) {
		CPRINTS("TCPC p%d restart failed!", port);
		return 0;
	}
#endif
	CPRINTS("TCPC p%d resumed!", port);
	set_state(port, TC_OBJ(port), tc_state_unattached_snk);

	return 0;
}

/**
 * ErrorRecovery
 *
 * Super State Entry Actions:
 *  Remove the terminations from CC
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
	/* Use cc_debounce timer as error recovery timer */
	tc[port].cc_debounce = get_time().val + PD_T_ERROR_RECOVERY;
	return 0;
}

static unsigned int tc_state_error_recovery_run(int port)
{
	if (tc[port].cc_debounce > 0 &&
				get_time().val > tc[port].cc_debounce) {
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
 *  Vconn Off
 *  Place Rd on CC
 *  Set power role to SINK
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

#ifdef CONFIG_CHARGE_MANAGER
	charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif

	/*
	 * Indicate that the port is disconnected so the board
	 * can restore state from any previous data swap.
	 */
	pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
	tc[port].pd_enable = 0;
	tc[port].next_role_swap = get_time().val + PD_T_DRP_SNK;
	return 0;
}

static unsigned int tc_state_unattached_snk_run(int port)
{
	int cc1;
	int cc2;

	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		CLR_FLAG(port, TC_FLAGS_HARD_RESET);

		tc_set_data_role(port, PD_ROLE_UFP);
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
		pe_ps_reset_complete(port);
#endif
        }

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	/*
	 * The port shall transition to AttachWait.SNK when a Source connection
	 * is detected, as indicated by the SNK.Rp state on at least one of its
	 * CC pins.
	 *
	 * A DRP shall transition to Unattached.SRC within tDRPTransition after
	 * the state of both CC pins is SNK.Open for tDRP − dcSRC.DRP ∙ tDRP.
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
 *  Vconn Off
 *  Place Rd on CC
 *  Set power role to SINK
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
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
#endif
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
		} else { /* new_cc_state == PD_CC_DEBUG_ACC */
			SET_FLAG(port, TC_FLAGS_TS_DTS_PARTNER);
			set_state(port, TC_OBJ(port),
						tc_state_dbg_acc_snk);
		}
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		hook_call_deferred(&pd_usb_billboard_deferred_data, PD_T_AME);
#endif
#endif
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

#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	if (CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS)) {
		/*
		 * Both CC1 and CC2 pins shall be independently terminated to
		 * ground through Rd.
		 */
		tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
		tcpm_set_cc(port, TYPEC_CC_RD);

		/* Change role to sink */
		tc_set_power_role(port, PD_ROLE_SINK);
		tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);

		/* 
		 * Maintain VCONN supply state, whether ON or OFF, and its
		 * data role / usb mux connections.
		 */
	} else
#endif
	{
		/* Get connector orientation */
		tcpm_get_cc(port, &cc1, &cc2);
		tc[port].polarity = get_snk_polarity(cc1, cc2);
		set_polarity(port, tc[port].polarity);

		/* 
		 * Initial data role for sink is UFP
        	 * This also sets the usb mux
        	 */
        	tc_set_data_role(port, PD_ROLE_UFP);

#if defined(CONFIG_CHARGE_MANAGER)
		tc[port].typec_curr =
			get_typec_current_limit(tc[port].polarity, cc1, cc2);
		typec_set_input_current_limit(port, tc[port].typec_curr,
							TYPE_C_VOLTAGE);
		charge_manager_update_dualrole(port, CAP_DEDICATED);
		tc[port].cc_state = (tc[port].polarity) ? cc2 : cc1;
#endif
	}

	/* Enable PD */
	tc[port].pd_enable = 1;
	tc[port].timeout = 0;
	tc[port].cc_debounce = 0;
	tc[port].ps_reset_state = PS_STATE0;
	return 0;
}

static unsigned int tc_state_attached_snk_run(int port)
{
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	/*
	 * Wait until PS swap is complete
	 */
	if (CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS))
		return 0;

	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		if (get_time().val < tc[port].timeout)
			return 0;

		switch (tc[port].ps_reset_state) {
		case PS_STATE0:
			tc_set_data_role(port, PD_ROLE_UFP);
			/* Clear the input current limit */
			pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
			charge_manager_set_ceil(port,
					CEIL_REQUESTOR_PD,
					CHARGE_CEIL_NONE);
#endif /* CONFIG_CHARGE_MANAGER */

#ifdef CONFIG_USBC_VCONN
			set_vconn(port, 0);
#endif
			tc[port].ps_reset_state = PS_STATE1;
			tc[port].timeout = get_time().val + 100*MSEC;
			return 0;
		case PS_STATE1:
			tc[port].timeout = 0;
			tc[port].ps_reset_state = PS_STATE0;
			CLR_FLAG(port, TC_FLAGS_HARD_RESET);
			pe_ps_reset_complete(port);
			break;
		case PS_STATE2:
			break;
		}
	}

	/*
	 * The sink will be powered off during a power role swap but we don't
	 * want to trigger a disconnect
	 */
	if (!CHK_FLAG(port, TC_FLAGS_POWER_OFF_SNK)) {
		/* Detach detection */
		if (!pd_is_vbus_present(port)) {
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
			return 0;
		}

		if (!pe_is_explicit_contract(port)) {
			int cc1;
			int cc2;

			/* Sink Power Sub-State */
			tcpm_get_cc(port, &cc1, &cc2);
#if defined(CONFIG_CHARGE_MANAGER)
			tc[port].typec_curr =
				get_typec_current_limit(tc[port].polarity, cc1, cc2);
			typec_set_input_current_limit(port, tc[port].typec_curr,
								TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port, CAP_DEDICATED);
#endif
		}
	}

	/*
	 * PD swap commands
	 */
	if (tc[port].pd_enable && (prl_get_local_state(port) == SM_RUN)) {
		/*
		 * Power Role Swap
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);

			/* Perform Power Role Swap */
			SET_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS);
			set_state(port, TC_OBJ(port), tc_state_attached_src);
			return 0;
		}

		/*
		 * Data Role Swap
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_DR_SWAP)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_DR_SWAP);

			/* Perform Data Role Swap */
			tc_set_data_role(port, !tc[port].data_role);
		}

#ifdef CONFIG_USBC_VCONN
		/*
		 * VCONN Swap
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_ON)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_ON);

			set_vconn(port, 1);
			pe_vconn_swap_complete(port);
		}
		else if (CHK_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_OFF)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_OFF);

			set_vconn(port, 0);
			pe_vconn_swap_complete(port);
		}
#endif
		/*
		 * If the port supports Charge-Through VCONN-Powered USB devices,
		 * and an explicit PD contract has failed to be negotiated, the
		 * port shall query the identity of the cable via USB PD on SOP’
		 */
		if (!pe_is_explicit_contract(port) &&
						CHK_FLAG(port, TC_FLAGS_CTVPD_DETECTED)) {
			/*
			 * A port that via SOP’ has detected an attached Charge-Through
			 * VCONN-Powered USB device shall transition to Unattached.SRC
			 * if an explicit PD contract has failed to be negotiated.
			 */
			/* CTVPD detected */
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
		}
	}

#else /* CONFIG_USB_PE_SM && CONFIG_USB_PRL_SM */

	/* Detach detection */
	if (!pd_is_vbus_present(port)) {
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
#endif
		set_state(port, TC_OBJ(port), tc_state_unattached_src);
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

		if (cc1 == TYPEC_CC_VOLT_RP_DEF || cc2 == TYPEC_CC_VOLT_RP_DEF)
                	new_cc_state = TYPEC_CC_VOLT_RP_DEF;
		else if (cc1 == TYPEC_CC_VOLT_RP_1_5 || cc2 == TYPEC_CC_VOLT_RP_1_5)
			new_cc_state = TYPEC_CC_VOLT_RP_1_5;
		else if (cc1 == TYPEC_CC_VOLT_RP_3_0 || cc2 == TYPEC_CC_VOLT_RP_3_0)
			new_cc_state = TYPEC_CC_VOLT_RP_3_0;
		else
			new_cc_state = TYPEC_CC_VOLT_OPEN;
			
		/* Debounce the cc state */
		if (new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = new_cc_state;
			tc[port].cc_debounce = get_time().val + PD_T_RP_VALUE_CHANGE;
			return 0;
		}

		if (tc[port].cc_debounce == 0 || get_time().val < tc[port].cc_debounce)
			return 0;

		tc[port].cc_debounce = 0;

#if defined(CONFIG_CHARGE_MANAGER)
		tc[port].typec_curr =
			get_typec_current_limit(tc[port].polarity, cc1, cc2);

		typec_set_input_current_limit(port,
					tc[port].typec_curr, TYPE_C_VOLTAGE);
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	}
#endif
#endif /* !(CONFIG_USB_PE_SM && CONFIG_USB_PRL_SM) */
	return 0;
}

static unsigned int tc_state_attached_snk_exit(int port)
{
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	CLR_FLAG(port, TC_FLAGS_POWER_OFF_SNK);

#ifdef CONFIG_USBC_VCONN
	/*
	 * If supplying VCONN, the port shall cease to supply it within
	 * tVCONNOFF of exiting Attached.SNK.
	 */
	if (!CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS) &&
					CHK_FLAG(port, TC_FLAGS_VCONN_ON))
		set_vconn(port, 0);
#endif
#endif

	/* Stop drawing power */
	pd_set_input_current_limit(port, 0, 0);
#ifdef CONFIG_CHARGE_MANAGER
	typec_set_input_current_limit(port, 0, 0);
	charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
#endif

	return 0;
}
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

static unsigned int tc_state_unoriented_dbg_acc_src_entry(int port)\
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
	 * A DRP, the port shall transition to Unattached.SNK when the SRC.Open state
	 * is detected on either the CC1 or CC2 pin.
	 */
	if (cc1 == TYPEC_CC_VOLT_OPEN) {
		/* Remove VBUS */
		pd_power_supply_reset(port);
		pd_set_input_current_limit(port, 0, 0);
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);

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
 *  Vconn Off
 *  Place Rp on CC
 *  Set power role to SOURCE
 */

static unsigned int tc_state_oriented_dbg_acc_src(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_oriented_dbg_acc_snk_sig[sig])(port);
	return SUPER(ret, sig, tc_state_cc_rp);
}

static unsigned int tc_state_oriented_dbg_acc_src_entry(int port)\
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
	int cc1;
	int cc2;
	int new_cc_state;

#if defined(CONFIG_CHARGE_MANAGER)
	if (pd_is_vbus_present(port)) {
		typec_set_input_current_limit(port, TYPE_C_CURRENT, TYPE_C_VOLTAGE);
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	} else {
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);
	}
#endif
	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (cc1 == TYPEC_CC_VOLT_OPEN || cc2 == TYPEC_CC_VOLT_OPEN)
		new_cc_state = PD_CC_NONE;

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		tc[port].cc_state = new_cc_state;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		return 0;
	}

	/*
	 * A DRP shall transition to Unattached.SRC when the state of the
	 * monitored CC1 or CC2 pin(s) is SRC.Open for at least tCCDebounce.
	 */
	if ((get_time().val < tc[port].cc_debounce) &&
			new_cc_state == PD_CC_NONE) {
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
#endif
		set_state(port, TC_OBJ(port), tc_state_unattached_src);
	}

	return 0;
}

static unsigned int tc_state_audio_acc_exit(int port)
{
#if defined(CONFIG_CHARGE_MANAGER)
	typec_set_input_current_limit(port, 0, 0);
	charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
#endif	
	return 0;
}

/**
 * Debug Accessory.SNK
 *
 * Super State Entry Actions:
 *  Vconn Off
 *  Place Rd on CC
 *  Set power role to SINK
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
	/*
	 * The port shall transition to Unattached.SNK when VBUS is no longer present.
	 */
	if (!pd_is_vbus_present(port)) {
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		pd_dfp_exit_mode(port, 0, 0);
#endif
#endif
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
}
	return 0;
}

/**
 * Unattached.SRC
 *
 * Super State Entry Actions:
 *  Vconn Off
 *  Place Rp on CC
 *  Set power role to SOURCE
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

#if defined(CONFIG_CHARGE_MANAGER)
	charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif
	tc_set_data_role(port, PD_ROLE_DFP);
	/*
	 * Indicate that the port is disconnected so the board
	 * can restore state from any previous data swap.
	 */
	pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
#ifdef CONFIG_USBC_SS_MUX
	usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
						     tc[port].polarity);
#endif
	tc[port].pd_enable = 0;
	tc[port].next_role_swap = get_time().val + PD_T_DRP_SRC;

	return 0;
}

static unsigned int tc_state_unattached_src_run(int port)
{
	int cc1;
	int cc2;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		CLR_FLAG(port, TC_FLAGS_HARD_RESET);
		tc_set_data_role(port, PD_ROLE_DFP);
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
		pe_ps_reset_complete(port);
#endif
	}

	/*
	 * The port shall transition to AttachWait.SRC when VBUS is vSafe0V and:
	 *   1) The SRC.Rd state is detected on either CC1 or CC2 pin or
	 *   2) The SRC.Ra state is detected on both the CC1 and CC2 pins.
	 *
	 * A DRP shall transition to Unattached.SNK within tDRPTransition after
	 * dcSRC.DRP ∙ tDRP
	 */
	if ((cc1 == TYPEC_CC_VOLT_RD || cc2 == TYPEC_CC_VOLT_RD) ||
			(cc1 == TYPEC_CC_VOLT_RA && cc2 == TYPEC_CC_VOLT_RA)) {
		set_state(port, TC_OBJ(port), tc_state_attach_wait_src);
	} else if (get_time().val > tc[port].next_role_swap) {
		set_state(port, TC_OBJ(port), tc_state_unattached_snk);
	}

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
	 * UnorientedDebugAccessory.SRC when VBUS is at vSafe0V and the SRC.Rd state
	 * is detected on both the CC1 and CC2 pins for at least tCCDebounce.
	 */
	if (!pd_is_vbus_present(port)) {
		if (new_cc_state == PD_CC_UFP_ATTACHED) {
			set_state(port, TC_OBJ(port), tc_state_attached_src);
			return 0;
		}
#if 0
		else if (new_cc_state == PD_CC_DEBUG_ACC) {
			set_state(port, TC_OBJ(port), tc_state_unoriented_dbg_acc_src);
			return 0;
		}
#endif
	}

	/*
	 * If the port supports Audio Adapter Accessory Mode, it shall transition
	 * to AudioAccessory when the SRC.Ra state is detected on both the CC1 and
	 * CC2 pins for at least tCCDebounce.
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

	if (CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS)) {
		/*
		 * Both CC1 and CC2 pins shall be independently terminated to
		 * ground through Rp.
		 */
		tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
		tcpm_set_cc(port, TYPEC_CC_RP);

		/* Change role to source */
		tc_set_power_role(port, PD_ROLE_SOURCE);
		tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);

		/* 
                 * Maintain VCONN supply state, whether ON or OFF, and its
                 * data role / usb mux connections.
                 */
        } else {
		/* Get connector orientation */
		tcpm_get_cc(port, &cc1, &cc2);
		tc[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);
		set_polarity(port, tc[port].polarity);

                /*
		 * Initial data role for sink is DFP
                 * This also sets the usb mux
                 */
                tc_set_data_role(port, PD_ROLE_DFP);

#ifdef CONFIG_USBC_VCONN
		/*
		 * Start sourcing Vconn before Vbus to ensure
		 * we are within USB Type-C Spec 1.3 tVconnON
		 */
		set_vconn(port, 1);
#endif

		/* Enable VBUS */
		if (pd_set_power_supply_ready(port)) {
			/* Stop sourcing Vconn if Vbus failed */
#ifdef CONFIG_USBC_VCONN
			set_vconn(port, 0);
#endif
#ifdef CONFIG_USBC_SS_MUX
			usb_mux_set(port, TYPEC_MUX_NONE,
				USB_SWITCH_DISCONNECT, tc[port].polarity);
#endif
		}

		tc[port].pd_enable = 0;
		tc[port].timeout = get_time().val + PD_POWER_SUPPLY_TURN_ON_DELAY;
	}

	tc[port].ps_reset_state = PS_STATE0;
	return 0;
}

static unsigned int tc_state_attached_src_run(int port)
{
	int cc1;
	int cc2;
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
	/*
	 * Wait until PS swap is complete
	 */
	if (CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS))
		return 0;

	/* Enable PD communications after power supply has fully turned on */
	if (tc[port].pd_enable == 0 && get_time().val > tc[port].timeout) {
		tc[port].pd_enable = 1;
		tc[port].timeout = 0;
	}

	if (tc[port].pd_enable == 0)
		return 0;

	/*
	 * Handle Hard Reset from Policy Engine
	 */
	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		if (get_time().val < tc[port].timeout)
			return 0;

		switch (tc[port].ps_reset_state) {
		case PS_STATE0:
			/* Set role to DFP */
			tc_set_data_role(port, PD_ROLE_DFP);
#ifdef CONFIG_USBC_VCONN
			/* Turn off VCONN */
			set_vconn(port, 0);
#endif
			/* Remove VBUS */
			tc_src_power_off(port);

			tc[port].ps_reset_state = PS_STATE1;
			tc[port].timeout = get_time().val +
					PD_POWER_SUPPLY_TURN_OFF_DELAY;
			return 0;
		case PS_STATE1:
			/* Enable VBUS */
			pd_set_power_supply_ready(port);

			tc[port].ps_reset_state = PS_STATE2;
			tc[port].timeout = get_time().val +
					PD_POWER_SUPPLY_TURN_ON_DELAY;
			return 0;
		case PS_STATE2:
#ifdef CONFIG_USBC_VCONN
			/* Turn on VCONN */
			set_vconn(port, 1);
#endif
			/* Tell Policy Engine Hard Reset is complete */
			pe_ps_reset_complete(port);

			CLR_FLAG(port, TC_FLAGS_HARD_RESET);
			tc[port].ps_reset_state = PS_STATE0;
			return 0;
		}
	}
#endif

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (tc[port].polarity)
		cc1 = cc2;

	/*
	 * When the SRC.Open state is detected on the monitored CC pin, a DRP shall
	 * transition to Unattached.SNK unless it strongly prefers the Source role.
	 * In that case, it shall transition to TryWait.SNK. This transition to
	 * TryWait.SNK is needed so that two devices that both prefer the Source
	 * role do not loop endlessly between Source and Sink. In other words, a DRP
	 * that would enter Try.SRC from AttachWait.SNK shall enter TryWait.SNK for
	 * a Sink detach from Attached.SRC.
	 */
	if (cc1 == TYPEC_CC_VOLT_OPEN) {
		set_state(port, TC_OBJ(port), tc_state_try_wait_snk);
		return 0;
	}

#if defined(CONFIG_USB_PRL_SM) && defined(CONFIG_USB_PE_SM)
	/*
	 * PD swap commands
	 */
	if (tc[port].pd_enable && (prl_get_local_state(port) == SM_RUN)) {
		/*
		 * Power Role Swap Request
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_PR_SWAP);

			/* Perform Power Role Swap */
			SET_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS);
			set_state(port, TC_OBJ(port), tc_state_attached_snk);
			return 0;
		}

		/*
		 * Data Role Swap Request
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_DR_SWAP)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_DR_SWAP);

			/* Perform Data Role Swap */
			tc_set_data_role(port, !tc[port].data_role);
		}

#ifdef CONFIG_USBC_VCONN
		/*
		 * VCONN Swap Request
		 */
		if (CHK_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_ON)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_ON);
			set_vconn(port, 1);
			pe_vconn_swap_complete(port);
		}
		else if (CHK_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_OFF)) {
			CLR_FLAG(port, TC_FLAGS_REQUEST_VC_SWAP_OFF);
			set_vconn(port, 0);
			pe_vconn_swap_complete(port);
		}
#endif

		/*
		 * A DRP that supports Charge-Through VCONN-Powered USB Devices
		 * shall transition to CTUnattached.SNK if the connected device
		 * identifies itself as a Charge-Through VCONN-Powered USB
		 * Device in its Discover Identity Command response.
		 */

		/*
		 * A DRP that supports Charge-Through VCONN-Powered USB Devices
		 * shall transition to CTUnattached.SNK if the connected device
		 * identifies itself as a Charge-Through VCONN-Powered USB
		 * Device in its Discover Identity Command response.
		 *
		 * If it detects that it is connected to a VCONN-Powered USB
		 * Device, the port may remove VBUS and discharge it to
		 * vSafe0V, while continuing to remain in this state with VCONN
		 * applied. 
		 */
		if (CHK_FLAG(port, TC_FLAGS_CTVPD_DETECTED)) {
			/* TC_FLAGS_CTVPD_DETECTED flag is cleard on exit */
			set_state(port, TC_OBJ(port), tc_state_ct_unattached_snk);
		}
        }
#endif
	return 0;
}

static unsigned int tc_state_attached_src_exit(int port)
{
#ifdef CONFIG_USBC_VCONN
	/*
	 * A port that is supplying VCONN shall cease to supply it within
	 * tVCONNOFF of exiting Attached.SRC, unless it is exiting as a
	 * result of a USB PD PR_Swap or is transitioning into the
	 * CTUnattached.SNK state
	 */
	if (!CHK_FLAG(port, TC_FLAGS_CTVPD_DETECTED) &&
				!CHK_FLAG(port, TC_FLAGS_PS_SWAP_IN_PROGRESS))
		set_vconn(port, 0);
#endif

	/* Clear CTVPD_DETECTED flag is set */
	CLR_FLAG(port, TC_FLAGS_CTVPD_DETECTED);

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
 *  Vconn Off
 *  Place Rp on CC
 *  Set power role to SOURCE
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
	/* Use pd_debounce timer for Try.SRC timeout */
	tc[port].pd_debounce = get_time().val + PD_T_TRY_TIMEOUT;
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
	 * The port shall transition to TryWait.SNK after tDRPTry and the SRC.Rd state
	 * has not been detected and VBUS is within vSafe0V, or after tTryTimeout and
	 * the SRC.Rd state has not been detected.
	 */
	if (new_cc_state == PD_CC_NONE) {
		if ((get_time().val > tc[port].try_wait_debounce &&
					!pd_is_vbus_present(port)) ||
					get_time().val > tc[port].pd_debounce) {
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
 *  Vconn Off
 *  Place Rd on CC
 *  Set power role to SINK
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
	tc[port].pd_enable = 0;
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
	 * The port shall transition to Unattached.SNK when the state of both of
	 * the CC1 and CC2 pins is SNK.Open for at least tPDDebounce.
	 */
	if (get_time().val > tc[port].pd_debounce) {
		if (new_cc_state == PD_CC_NONE) {
#if defined(CONFIG_USB_PE_SM) && defined(CONFIG_USB_PRL_SM)
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
			pd_dfp_exit_mode(port, 0, 0);
#endif
#endif
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
			return 0;
		}
	}

	/* 
	 * The port shall transition to Attached.SNK after tCCDebounce if or when
	 * VBUS is detected. 
	 */
	if (get_time().val > tc[port].try_wait_debounce) {
		if (pd_is_vbus_present(port))
			set_state(port, TC_OBJ(port), tc_state_attached_snk);
	}

	return 0;
}

#if defined(CONFIG_USB_PRL_SM) && defined(CONFIG_USB_PE_SM)
/*
 * CTUnattached.SNK
 */
static unsigned int tc_state_ct_unattached_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_unattached_snk_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_ct_unattached_snk_entry(int port)
{
	tc[port].state_id = CTUNATTACHED_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/*
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rd.
	 */
	tcpm_select_rp_value(port, CONFIG_USB_PD_PULLUP);
	tcpm_set_cc(port, TYPEC_CC_RD);
	tc[port].cc_state = PD_CC_UNSET;

	/* Set power role to sink */
        tc_set_power_role(port, PD_ROLE_SINK);
	tcpm_set_msg_header(port, tc[port].power_role, tc[port].data_role);

	/*
	 * The policy engine is in the disabled state. Disable PD and
	 * re-enable it
	 */
	tc[port].pd_enable = 0;

	tc[port].timeout = get_time().val + PD_POWER_SUPPLY_TURN_ON_DELAY;

	return 0;
}

static unsigned int tc_state_ct_unattached_snk_run(int port)
{
	int cc1;
	int cc2;
	int new_cc_state;

	if (tc[port].timeout > 0 && get_time().val > tc[port].timeout) {
		tc[port].pd_enable = 1;
		tc[port].timeout = 0;
	}

	if (tc[port].timeout > 0)
		return 0;

	/* Wait until Protocol Layer is ready */
	if (prl_get_local_state(port) != SM_RUN)
		return 0;
	
	/*
	 * Hard Reset is sent when the PE layer is disabled due to a
	 * CTVPD connection.
	 */
	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		CLR_FLAG(port, TC_FLAGS_HARD_RESET);
		/* Nothing to do. Just signal hard reset completion */
		pe_ps_reset_complete(port);
        }

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
		tc[port].cc_debounce = get_time().val + PD_T_VPD_DETACH;
        }

	/*
	 * The port shall transition to Unattached.SNK if the state of the CC pin is
	 * SNK.Open for tVPDDetach after VBUS is vSafe0V.
	 */
	if (get_time().val > tc[port].cc_debounce) {
		if (new_cc_state == PD_CC_NONE && !pd_is_vbus_present(port)) {
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
			pd_dfp_exit_mode(port, 0, 0);
#endif
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
			return 0;
		}
	}

	/*
	 * The port shall transition to CTAttached.SNK when VBUS is detected.
	 */
	if (pd_is_vbus_present(port))
		set_state(port, TC_OBJ(port), tc_state_ct_attached_snk);

	return 0;
}

/**
 * CTAttached.SNK
 */
static unsigned int tc_state_ct_attached_snk(int port, enum signal sig)
{
	int ret;

	ret = (*tc_state_ct_attached_snk_sig[sig])(port);
	return SUPER(ret, sig, 0);
}

static unsigned int tc_state_ct_attached_snk_entry(int port)
{
	tc[port].state_id = CTATTACHED_SNK;
	CPRINTS("C%d: %s", port, tc_state_names[tc[port].state_id]);

	/* The port shall reject a VCONN swap request. */
	SET_FLAG(port, TC_FLAGS_REJECT_VCONN_SWAP);

	return 0;
}

static unsigned int tc_state_ct_attached_snk_run(int port)
{
	int cc1;
	int cc2;

	/*
	 * Hard Reset is sent when the PE layer is disabled due to a
	 * CTVPD connection.
	 */
	if (CHK_FLAG(port, TC_FLAGS_HARD_RESET)) {
		CLR_FLAG(port, TC_FLAGS_HARD_RESET);
		/* Nothing to do. Just signal hard reset completion */
		pe_ps_reset_complete(port);
        }

	/*
	 * A port that is not in the process of a USB PD Hard Reset shall
	 * transition to CTUnattached.SNK within tSinkDisconnect when VBUS
	 * falls below vSinkDisconnect 
	 */
	if (!pd_is_vbus_present(port)) {
		set_state(port, TC_OBJ(port), tc_state_ct_unattached_snk);
		return 0;
	}

	/*
	 *  The port shall operate in one of the Sink Power Sub-States
	 *  and remain within the Sink Power Sub-States, until either VBUS is
	 *  removed or a USB PD contract is established with the source.
	 */
	if (!pe_is_explicit_contract(port)) {
		/* Sink Power Sub-State */
		tcpm_get_cc(port, &cc1, &cc2);
#if defined(CONFIG_CHARGE_MANAGER)
		tc[port].typec_curr =
                                get_typec_current_limit(tc[port].polarity, cc1, cc2);
                typec_set_input_current_limit(port, tc[port].typec_curr,
                                                                TYPE_C_VOLTAGE);
                charge_manager_update_dualrole(port, CAP_DEDICATED);
#endif
	}

	return 0;
}

static unsigned int tc_state_ct_attached_snk_exit(int port)
{
	CLR_FLAG(port, TC_FLAGS_REJECT_VCONN_SWAP);
	return 0;
}
#endif /* CONFIG_USB_PE_SM && CONFIG_USB_PRL_SM */

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
#ifdef CONFIG_USBC_VCONN
	/* Disable VCONN */
	set_vconn(port, 0);
#endif

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
#ifdef CONFIG_USBC_VCONN
	/* Disable VCONN */
	set_vconn(port, 0);
#endif

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
#ifdef CONFIG_USBC_VCONN
	/* Disable VCONN */
	set_vconn(port, 0);
#endif
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
