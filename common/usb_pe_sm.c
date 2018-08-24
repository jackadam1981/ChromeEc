/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "battery_smart.h"
#include "board.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "registers.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "usb_charge.h"
#include "usb_mux.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usb_prl_sm.h"
#include "usbc_ppc.h"
#include "tcpm.h"
#include "usb_emsg.h"
#include "usb_pe_sm.h"
#include "usb_sm.h"
#include "usb_tc_sm.h"
#include "version.h"
#include "vboot.h"

#define N_HARD_RESET_COUNT 2
#define N_CAPS_COUNT 50

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#endif

/* Last received source cap */
static uint32_t pd_src_caps[CONFIG_USB_PD_PORT_COUNT][PDO_MAX_OBJECTS];
static int pd_src_cap_cnt[CONFIG_USB_PD_PORT_COUNT];

#define SNK_OBJ(port) (SM_OBJ(pe_snk[port]))
#define SRC_OBJ(port) (SM_OBJ(pe_src[port]))


static struct policy_engine_sink {
	/* struct sm_obj must be first. */
	struct sm_obj obj;
	uint32_t sm_flags;
	uint64_t timeout;
} pe_snk[CONFIG_USB_PD_PORT_COUNT];

static struct policy_engine_source {
	/* struct sm_obj must be first. */
	struct sm_obj obj;
	uint32_t sm_flags;
} pe_src[CONFIG_USB_PD_PORT_COUNT];

static struct policy_engine {
	uint32_t rev;
	uint32_t sm_flags;
	uint32_t flags;
/* last requested voltage PDO index */
	int requested_idx;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	/* Current limit / voltage based on the last request message */
	uint32_t curr_limit;
	uint32_t supply_voltage;
	/* Signal charging update that affects the port */
	int new_power_request;
	/* Store previously requested voltage request */
	int prev_request_mv;
#endif
	uint32_t caps_counter;
	uint32_t hard_reset_counter;
	int evt_timeout;
	uint64_t sender_response_timer;
	uint64_t no_response_timer;
	uint64_t swap_source_start_timer;
	uint64_t source_capability_timer;
	uint64_t ps_hard_reset_timer;
} pd[CONFIG_USB_PD_PORT_COUNT];

/* Cap on the max voltage requested as a sink (in millivolts) */
static unsigned int max_request_mv = PD_MAX_VOLTAGE_MV; /* no cap */



/* Policy Engine states */
/* Source Port States */
static void pe_src_startup(int port, int sig);

static void pe_src_discovery(int port, int sig);
static void pe_src_send_capabilities(int port, int sig);
static void pe_src_negotiate_capability(int port, int sig);
static void pe_src_transition_supply(int port, int sig);
static void pe_src_ready(int port, int sig);
static void pe_src_capability_response(int port, int sig);
static void pe_src_hard_reset(int port, int sig);
static void pe_src_hard_reset_received(int port, int sig);
static void pe_src_transition_to_default(int port, int sig);
static void pe_src_wait_new_capabilities(int port, int sig);

/* Source Port Soft Reset States */
static void pe_src_send_soft_reset(int port, int sig);
static void pe_src_soft_reset(int port, int sig);

/* Source Port Not Supported States */
static void pe_send_not_supported(int port, int sig);
#if 0
/* Source Port Ping */
static void pe_src_ping(int port, int sig);

/* Source Port Source Alert */
static void pe_src_send_source_alert(int port, int sig);

/* Source Port Sink Alert */
static void pe_src_sink_alert_received(int port, int sig);

/* Source Port Give Source Status */
static void pe_src_give_source_status(int port, int sig);

/* Source Port Get Sink Status */
static void pe_src_get_sink_status(int port, int sig);

/* Source Port Give Source Capabilities Extended */
static void pe_src_give_source_cap_ext(int port, int sig);

/* Source Port Give PPS Status */
static void pe_src_give_pps_status(int port, int sig);
#endif

/* Sink Port States */
static void pe_snk_startup(int port, int sig);
static void pe_snk_wait_for_capabilities(int port, int sig);
static void pe_snk_evaluate_capability(int port);

static void pe_snk_select_capability(int port, int sig);
static void pe_snk_transition_sink(int port, int sig);
static void pe_snk_ready(int port, int sig);
static void pe_snk_hard_reset(int port, int sig);
static void pe_snk_transition_to_default(int port, int sig);
static void pe_snk_give_sink_cap(int port, int sig);

/* Sink Port Soft Reset States */
static void pe_snk_send_soft_reset(int port, int sig);
static void pe_snk_soft_reset(int port, int sig);


/* Give Battery Capabilities */
static void pe_give_battery_cap(int port, int sig);

/* Give Battery Status */
static void pe_give_battery_status(int port, int sig);

#if 0
/* DFP to UFP Data Role Swap */
static void pe_drs_dfp_ufp_evaluate_swap(int port, int sig);
static void pe_drs_dfp_ufp_accept_swap(int port, int sig);
static void pe_drs_dfp_ufp_change_to_ufp(int port, int sig);
static void pe_drs_dfp_ufp_send_swap(int port, int sig);
static void pe_drs_dfp_ufp_reject_swap(int port, int sig);

/* UFP to DFP Data Role Swap */
static void pe_drs_ufp_dfp_evaluate_swap(int port, int sig);
static void pe_drs_ufp_dfp_accept_swap(int port, int sig);
static void pe_drs_ufp_dfp_change_to_dfp(int port, int sig);
static void pe_drs_ufp_dfp_send_swap(int port, int sig);
static void pe_drs_ufp_dfp_reject_swap(int port, int sig);

/* Source to Sink Power Role Swap */
static void pe_prs_src_snk_evaluate_swap(int port, int sig);
static void pe_prs_src_snk_accept_swap(int port, int sig);
static void pe_prs_src_snk_transition_to_off(int port, int sig);
static void pe_prs_src_snk_assert_rd(int port, int sig);
static void pe_prs_src_snk_wait_source_on(int port, int sig);
static void pe_prs_src_snk_send_swap(int port, int sig);
static void pe_prs_src_snk_reject_swap(int port, int sig);

/* Sink to Source Power Role Swap */
static void pe_prs_snk_src_evaluate_swap(int port, int sig);
static void pe_prs_snk_src_accept_swap(int port, int sig);
static void pe_prs_snk_src_transition_to_off(int port, int sig);
static void pe_prs_snk_src_assert_rp(int port, int sig);
static void pe_prs_snk_src_source_on(int port, int sig);
static void pe_prs_snk_src_send_swap(int port, int sig);
static void pe_prs_snk_src_reject_swap(int port, int sig);

/* Source to Sink Fast Role Swap */
static void pe_frs_src_snk_cc_signal(int port, int sig);
static void pe_frs_src_snk_evaluate_swap(int port, int sig);
static void pe_frs_src_snk_accept_swap(int port, int sig);
static void pe_frs_src_snk_transition_to_off(int port, int sig);
static void pe_frs_src_snk_assert_rd(int port, int sig);
static void pe_frs_src_snk_wait_source_on(int port, int sig);

/* Sink to Source Fast Role Swap */
static void pe_frs_snk_src_start_ams(int port, int sig);
static void pe_frs_snk_src_send_swap(int port, int sig);
static void pe_frs_snk_src_transition_to_off(int port, int sig);
static void pe_frs_snk_src_vbus_applied(int port, int sig);
static void pe_frs_snk_src_assert_rp(int port, int sig);
static void pe_frs_snk_src_source_on(int port, int sig);

/* Dual-Role Source Port Get Source Capabilities */
static void pe_dr_src_get_source_cap(int port, int sig);
/* Dual-Role Source Port Give Sink Capabilities */
static void pe_dr_src_give_sink_cap(int port, int sig);
/* Dual-Role Sink Port Get Sink Capabilities */
static void pe_dr_snk_get_sink_cap(int port, int sig);
/* Dual-Role Sink Port Give Source Capabilities */
static void pe_dr_snk_give_source_cap(int port, int sig);

/* Dual-Role Source Port Get Source Capabilities Extended */
static void pe_dr_src_get_source_cap_ext(int port, int sig);

/* Dual-Role Sink Port Give Source Capabilities Extended */
static void pe_dr_snk_give_source_cap_ext(int port, int sig);

/* USB Type-C V CONN Swap */
static void pe_vcs_send_swap(int port, int sig);
static void pe_vcs_evaluate_swap(int port, int sig);
static void pe_vcs_accept_swap(int port, int sig);
static void pe_vcs_reject_swap(int port, int sig);
static void pe_vcs_wait_for_vconn(int port, int sig);
static void pe_vcs_turn_off_vconn(int port, int sig);
static void pe_vcs_turn_on_vconn(int port, int sig);
static void pe_vcs_send_ps_rdy(int port, int sig);

/* Initiator to Port Structured VDM Discover Identity */
static void pe_init_port_vdm_identity_request(int port, int sig);
static void pe_init_port_vdm_identity_acked(int port, int sig);
static void pe_init_port_vdm_identity_naked(int port, int sig);

/* Initiator Structured VDM Discover SVIDs */
static void pe_init_vdm_svids_request(int port, int sig);
static void pe_init_vdm_svids_acked(int port, int sig);
static void pe_init_vdm_svids_naked(int port, int sig);

/* Initiator Structured VDM Discover Modes */
static void pe_init_vdm_modes_request(int port, int sig);
static void pe_init_vdm_modes_acked(int port, int sig);
static void pe_init_vdm_modes_naked(int port, int sig);

/* Initiator Structured VDM Attention */
static void pe_init_vdm_attention_request(int port, int sig);

/* Responder Structured VDM Discovery Identity */
static void pe_resp_vdm_get_identity(int port, int sig);
static void pe_resp_vdm_send_identity(int port, int sig);
static void pe_resp_vdm_get_identity_nak(int port, int sig);

/* Responder Structured VDM Discovery SVIDs */
static void pe_resp_vdm_get_svids(int port, int sig);
static void pe_resp_vdm_send_svids(int port, int sig);
static void pe_resp_vdm_get_svids_nak(int port, int sig);

/* Responder Structured VDM Discovery Modes */
static void pe_resp_vdm_get_modes(int port, int sig);
static void pe_resp_vdm_send_modes(int port, int sig);
static void pe_resp_vdm_get_modes_nak(int port, int sig);

/* Receiving a Structured VDM Attention*/
static void pe_rcv_vdm_attention_request(int port, int sig);

/* DFP Structured VDM Mode Entry */
static void pe_dfp_vdm_mode_entry_request(int port, int sig);
static void pe_dfp_vdm_mode_entry_acked(int port, int sig);
static void pe_dfp_vdm_mode_entry_naked(int port, int sig);
/* DFP Structured VDM Mode Exit */
static void pe_dfp_vdm_mode_exit_request(int port, int sig);
static void pe_dfp_vdm_mode_exit_acked(int port, int sig);
/* UFP Structured VDM Enter Mode */
static void pe_ufp_vdm_evaluate_mode_entry(int port, int sig);
static void pe_ufp_vdm_mode_entry_ack(int port, int sig);
static void pe_ufp_vdm_mode_entry_nak(int port, int sig);
/*  UFP Structured VDM Exit Mode */
static void pe_ufp_vdm_mode_exit(int port, int sig);
static void pe_ufp_vdm_mode_exit_ack(int port, int sig);
static void pe_ufp_vdm_mode_exit_nak(int port, int sig);

/* Source Startup Structured VDM Discover Identity */
static void pe_src_vdm_identity_request(int port, int sig);
static void pe_src_vdm_identity_acked(int port, int sig);
static void pe_src_vdm_identity_naked(int port, int sig);
/* BIST Carrier Mode */
static void pe_bist_carrier_mode(int port, int sig);
/* USB Type-C referenced states */
static void error_recovery(int port, int sig);

#endif

int pd_alt_mode(int port, uint16_t svid)
{
	return 0;
}

int pd_dfp_dp_get_pin_mode(int port, uint32_t status)
{
	return 0;
}

void pe_got_soft_reset(int port)
{
	if (tc_get_power_role(port) == PD_ROLE_SOURCE)
		set_state(port, SRC_OBJ(port), pe_src_soft_reset);
	else
		set_state(port, SNK_OBJ(port), pe_snk_soft_reset);
}

void pe_got_hard_reset(int port)
{
	if (tc_get_power_role(port) == PD_ROLE_SOURCE)
		set_state(port, SRC_OBJ(port), pe_src_hard_reset_received);
	else
		set_state(port, SNK_OBJ(port), pe_snk_transition_to_default);
}

int pe_pd_capable(int port)
{
	return pd[port].flags & PD_FLAGS_PREVIOUS_PD_CONN;
}

void pe_hard_reset_sent(int port)
{
	pd[port].sm_flags |= SM_FLAGS_HARD_RESET_COMPLETE;
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
}

void pe_get_source_cap(int port)
{
	pd[port].sm_flags |= SM_UPDATE_REMOTE_CAPS;
}

void pe_message_sent(int port)
{
	pd[port].sm_flags |= SM_FLAGS_TX_COMPLETE;
}

void pe_report_error(int port, enum pe_error e)
{
	/* FIXME */
	switch (e) {
	case ERR_RCH_CHUNKED:
	case ERR_RCH_MSG_REC:
	case ERR_TCH_CHUNKED:
		break;
	case ERR_TCH_XMIT:
		pd[port].sm_flags |= SM_FLAGS_TX_COMPLETE;
		break;
	case ERR_PRL_TX:
		if (tc_get_power_role(port) == PD_ROLE_SOURCE)
			set_state(port, SRC_OBJ(port), pe_src_send_soft_reset);
		else
			set_state(port, SNK_OBJ(port), pe_snk_send_soft_reset);
		break;
	}
}

void pe_pass_up_message(int port)
{
	pd[port].sm_flags |= SM_FLAGS_MSG_RECEIVED;
	task_set_event(PD_PORT_TO_TASK_ID(port), PD_EVENT_SM, 0);
}

static struct pd_policy pe[CONFIG_USB_PD_PORT_COUNT];

void pd_dfp_pe_init(int port)
{
}

int pd_charge_from_device(uint16_t vid, uint16_t pid)
{
	/* TODO: rewrite into table if we get more of these */
	/*
	 * White-list Apple charge-through accessory since it doesn't set
	 * externally powered bit, but we still need to charge from it when
	 * we are a sink.
	 */
	return (vid == USB_VID_APPLE && (pid == 0x1012 || pid == 0x1013));
}

uint16_t pd_get_identity_vid(int port)
{
	return PD_IDH_VID(pe[port].identity[0]);
}

uint16_t pd_get_identity_pid(int port)
{
	return PD_PRODUCT_PID(pe[port].identity[2]);
}


static void pd_usb_billboard_deferred(void)
{
#if defined(CONFIG_USB_PD_ALT_MODE) && !defined(CONFIG_USB_PD_ALT_MODE_DFP) \
	&& !defined(CONFIG_USB_PD_SIMPLE_DFP) && defined(CONFIG_USB_BOS)

	/*
	 * TODO(tbroch)
	 * 1. Will we have multiple type-C port UFPs
	 * 2. Will there be other modes applicable to DFPs besides DP
	 */
	if (!pd_alt_mode(0, USB_SID_DISPLAYPORT))
		usb_connect();
#endif
}
DECLARE_DEFERRED(pd_usb_billboard_deferred);

#ifdef CONFIG_USB_PD_DISCHARGE
void pd_set_vbus_discharge(int port, int enable)
{
	static struct mutex discharge_lock[CONFIG_USB_PD_PORT_COUNT];

	mutex_lock(&discharge_lock[port]);
	enable &= !board_vbus_source_enabled(port);
#ifdef CONFIG_USB_PD_DISCHARGE_GPIO
	if (!port)
		gpio_set_level(GPIO_USB_C0_DISCHARGE, enable);
#if CONFIG_USB_PD_PORT_COUNT > 1
	else
		gpio_set_level(GPIO_USB_C1_DISCHARGE, enable);
#endif /* CONFIG_USB_PD_PORT_COUNT */
#elif defined(CONFIG_USB_PD_DISCHARGE_TCPC)
	tcpc_discharge_vbus(port, enable);
#elif defined(CONFIG_USB_PD_DISCHARGE_PPC)
	ppc_discharge_vbus(port, enable);
#else
#error "PD discharge implementation not defined"
#endif
	mutex_unlock(&discharge_lock[port]);
}
#endif /* CONFIG_USB_PD_DISCHARGE */


void pe_init(int port)
{
	pd[port].rev = PD_REV30;
	pd[port].evt_timeout = 5*MSEC;

	pd[port].sm_flags = 0;
	pe_src[port].sm_flags = 0;
	pe_snk[port].sm_flags = 0;

	/* Reset ProtocolLayer */
	prl_init(port);

	init_state(port, SNK_OBJ(port), pe_snk_startup);
	init_state(port, SRC_OBJ(port), pe_src_startup);
}

int policy_engine(int port, int evt)
{
	if (tc_get_power_role(port) == PD_ROLE_SINK)
		pe_snk[port].obj.task_state(port, RUN_SIG);
	else if (tc_get_power_role(port) == PD_ROLE_SOURCE)
		pe_src[port].obj.task_state(port, RUN_SIG);

	/* Return new event timeout */
	return pd[port].evt_timeout;
}

static int send_source_cap(int port)
{
#if defined(CONFIG_USB_PD_DYNAMIC_SRC_CAP) || \
		defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT)
	const uint32_t *src_pdo;
	const int src_pdo_cnt = charge_manager_get_source_pdo(&src_pdo, port);
#else
	const uint32_t *src_pdo = pd_src_pdo;
	const int src_pdo_cnt = pd_src_pdo_cnt;
#endif

	if (src_pdo_cnt == 0) {
		/* No source capabilities defined, sink only */
		return prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	}

	emsg[port].rev = pd[port].rev;
	emsg[port].len = src_pdo_cnt * 4;
	memcpy(emsg[port].buf, (uint8_t *)src_pdo, emsg[port].len);
	return prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_SOURCE_CAP);
}

static void pd_update_pdo_flags(int port, uint32_t pdo)
{
#ifdef CONFIG_CHARGE_MANAGER
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	int charge_whitelisted =
		(tc_get_power_role(port) == PD_ROLE_SINK &&
			pd_charge_from_device(pd_get_identity_vid(port),
			pd_get_identity_pid(port)));
#else
	const int charge_whitelisted = 0;
#endif
#endif

	/* can only parse PDO flags if type is fixed */
	if ((pdo & PDO_TYPE_MASK) != PDO_TYPE_FIXED)
		return;

#ifdef CONFIG_USB_PD_DUAL_ROLE
	if (pdo & PDO_FIXED_DUAL_ROLE)
		pd[port].sm_flags |= SM_FLAGS_PARTNER_DR_POWER;
	else
		pd[port].sm_flags &= ~SM_FLAGS_PARTNER_DR_POWER;

	if (pdo & PDO_FIXED_EXTERNAL)
		pd[port].sm_flags |= SM_FLAGS_PARTNER_EXTPOWER;
	else
		pd[port].sm_flags &= ~SM_FLAGS_PARTNER_EXTPOWER;

	if (pdo & PDO_FIXED_COMM_CAP)
		pd[port].sm_flags |= SM_FLAGS_PARTNER_USB_COMM;
	else
		pd[port].sm_flags &= ~SM_FLAGS_PARTNER_USB_COMM;
#endif

	if (pdo & PDO_FIXED_DATA_SWAP)
		pd[port].sm_flags |= SM_FLAGS_PARTNER_DR_DATA;
	else
		pd[port].sm_flags &= ~SM_FLAGS_PARTNER_DR_DATA;

#ifdef CONFIG_CHARGE_MANAGER
	/*
	 * Treat device as a dedicated charger (meaning we should charge
	 * from it) if it does not support power swap, or if it is externally
	 * powered, or if we are a sink and the device identity matches a
	 * charging white-list.
	 */
	if (!(pd[port].sm_flags & SM_FLAGS_PARTNER_DR_POWER) ||
		(pd[port].sm_flags & SM_FLAGS_PARTNER_EXTPOWER) ||
		charge_whitelisted)
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	else
		charge_manager_update_dualrole(port, CAP_DUALROLE);
#endif
}

__attribute__((weak)) int pd_board_check_request(uint32_t rdo, int pdo_cnt)
{
	int idx = RDO_POS(rdo);

	/* Check for invalid index */
	return (!idx || idx > pdo_cnt) ?
		EC_ERROR_INVAL : EC_SUCCESS;
}

int pd_check_requested_voltage(uint32_t rdo, const int port)
{
	int max_ma = rdo & 0x3FF;
	int op_ma = (rdo >> 10) & 0x3FF;
	int idx = RDO_POS(rdo);
	uint32_t pdo;
	uint32_t pdo_ma;
#if defined(CONFIG_USB_PD_DYNAMIC_SRC_CAP) || \
	defined(CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT)
	const uint32_t *src_pdo;
	const int pdo_cnt = charge_manager_get_source_pdo(&src_pdo, port);
#else
	const uint32_t *src_pdo = pd_src_pdo;
	const int pdo_cnt = pd_src_pdo_cnt;
#endif

	/* Board specific check for this request */
	if (pd_board_check_request(rdo, pdo_cnt))
		return EC_ERROR_INVAL;

	/* check current ... */
	pdo = src_pdo[idx - 1];
	pdo_ma = (pdo & 0x3ff);
	if (op_ma > pdo_ma)
		return EC_ERROR_INVAL; /* too much op current */
	if (max_ma > pdo_ma && !(rdo & RDO_CAP_MISMATCH))
		return EC_ERROR_INVAL; /* too much max current */

	CPRINTF("Requested %d V %d mA (for %d/%d mA)\n",
		((pdo >> 10) & 0x3ff) * 50, (pdo & 0x3ff) * 10,
		op_ma * 10, max_ma * 10);

	/* Accept the requested voltage */
	return EC_SUCCESS;
}


int pd_find_pdo_index(int port, int max_mv, uint32_t *selected_pdo)
{
	int i, uw, mv, ma;
	int ret = 0;
	int cur_uw = 0;
	int prefer_cur;
	const uint32_t *src_caps = pd_src_caps[port];

	int __attribute__((unused)) cur_mv = 0;

	/* max voltage is always limited by this boards max request */
	max_mv = MIN(max_mv, PD_MAX_VOLTAGE_MV);

	/* Get max power that is under our max voltage input */
	for (i = 0; i < pd_src_cap_cnt[port]; i++) {
		/* its an unsupported Augmented PDO (PD3.0) */
		if ((src_caps[i] & PDO_TYPE_MASK) == PDO_TYPE_AUGMENTED)
			continue;

		mv = ((src_caps[i] >> 10) & 0x3FF) * 50;
		/* Skip invalid voltage */
		if (!mv)
			continue;
		/* Skip any voltage not supported by this board */
		if (!pd_is_valid_input_voltage(mv))
			continue;

		if ((src_caps[i] & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
			uw = 250000 * (src_caps[i] & 0x3FF);
		} else {
			ma = (src_caps[i] & 0x3FF) * 10;
			ma = MIN(ma, PD_MAX_CURRENT_MA);
			uw = ma * mv;
		}

		if (mv > max_mv)
			continue;
		uw = MIN(uw, PD_MAX_POWER_MW * 1000);
		prefer_cur = 0;

		/* Apply special rules in case of 'tie' */
#ifdef PD_PREFER_LOW_VOLTAGE
		if (uw == cur_uw && mv < cur_mv)
			prefer_cur = 1;
#elif defined(PD_PREFER_HIGH_VOLTAGE)
		if (uw == cur_uw && mv > cur_mv)
			prefer_cur = 1;
#endif
		/* Prefer higher power, except for tiebreaker */
		if (uw > cur_uw || prefer_cur) {
			ret = i;
			cur_uw = uw;
			cur_mv = mv;
		}
	}

	if (selected_pdo)
		*selected_pdo = src_caps[ret];

	return ret;
}

/**
 * Extract power information out of a Power Data Object (PDO)
 *
 * @pdo Power data object
 * @ma Current we can request from that PDO
 * @mv Voltage of the PDO
 */
void pd_extract_pdo_power(uint32_t pdo, uint32_t *ma, uint32_t *mv)
{
	int max_ma, uw;

	*mv = ((pdo >> 10) & 0x3FF) * 50;

	if (*mv == 0) {
		CPRINTF("ERR:PDO mv=0\n");
		*ma = 0;
		return;
	}

	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		uw = 250000 * (pdo & 0x3FF);
		max_ma = 1000 * MIN(1000 * uw, PD_MAX_POWER_MW) / *mv;
	} else {
		max_ma = 10 * (pdo & 0x3FF);
		max_ma = MIN(max_ma, PD_MAX_POWER_MW * 1000 / *mv);
	}

	*ma = MIN(max_ma, PD_MAX_CURRENT_MA);
}

void pd_process_source_cap(int port, int cnt, uint32_t *src_caps)
{
#ifdef CONFIG_CHARGE_MANAGER
	uint32_t ma, mv, pdo;
#endif
	int i;

	pd_src_cap_cnt[port] = cnt;
	for (i = 0; i < cnt; i++)
		pd_src_caps[port][i] = *src_caps++;

#ifdef CONFIG_CHARGE_MANAGER
	/* Get max power info that we could request */
	pd_find_pdo_index(port, PD_MAX_VOLTAGE_MV, &pdo);
	pd_extract_pdo_power(pdo, &ma, &mv);

	/* Set max. limit, but apply 500mA ceiling */
	charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, PD_MIN_MA);
	pd_set_input_current_limit(port, ma, mv);
#endif
}

int pd_build_request(int port, uint32_t *rdo, uint32_t *ma, uint32_t *mv,
		enum pd_request_type req_type)
{
	uint32_t pdo;
	int pdo_index, flags = 0;
	int uw;
	int max_or_min_ma;
	int max_or_min_mw;

	if (req_type == PD_REQUEST_VSAFE5V) {
		/* src cap 0 should be vSafe5V */
		pdo_index = 0;
		pdo = pd_src_caps[port][0];
	} else {
		/* find pdo index for max voltage we can request */
		pdo_index = pd_find_pdo_index(port, max_request_mv, &pdo);
	}

	pd_extract_pdo_power(pdo, ma, mv);
	uw = *ma * *mv;
	/* Mismatch bit set if less power offered than the operating power */
	if (uw < (1000 * PD_OPERATING_POWER_MW))
		flags |= RDO_CAP_MISMATCH;

#ifdef CONFIG_USB_PD_GIVE_BACK
	/* Tell source we are give back capable. */
	flags |= RDO_GIVE_BACK;

	/*
	 * BATTERY PDO: Inform the source that the sink will reduce
	 * power to this minimum level on receipt of a GotoMin Request.
	 */
	max_or_min_mw = PD_MIN_POWER_MW;

	/*
	 * FIXED or VARIABLE PDO: Inform the source that the sink will reduce
	 * current to this minimum level on receipt of a GotoMin Request.
	 */
	max_or_min_ma = PD_MIN_CURRENT_MA;
#else
	/*
	 * Can't give back, so set maximum current and power to operating
	 * level.
	 */
	max_or_min_ma = *ma;
	max_or_min_mw = uw / 1000;
#endif

	if ((pdo & PDO_TYPE_MASK) == PDO_TYPE_BATTERY) {
		int mw = uw / 1000;
		*rdo = RDO_BATT(pdo_index + 1, mw, max_or_min_mw, flags);
	} else {
		*rdo = RDO_FIXED(pdo_index + 1, *ma, max_or_min_ma, flags);
	}
	return EC_SUCCESS;
}


static int send_request(int port, uint32_t rdo)
{
	emsg[port].rev = pd[port].rev;
	emsg[port].len = 4;
	memcpy(emsg[port].buf, (uint8_t *)&rdo, emsg[port].len);

	return prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_REQUEST);
}

static int pd_send_request_msg(int port, int always_send_request)
{
	uint32_t rdo, curr_limit, supply_voltage;
	int res;

#ifdef CONFIG_CHARGE_MANAGER
	int charging = (charge_manager_get_active_charge_port() == port);
#else
	const int charging = 1;
#endif

#ifdef CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED
	int max_request_allowed = pd_is_max_request_allowed();
#else
	const int max_request_allowed = 1;
#endif

	/* Clear new power request */
	pd[port].new_power_request = 0;

	/* Build and send request RDO */
	/*
	 * If this port is not actively charging or we are not allowed to
	 * request the max voltage, then select vSafe5V
	 */
	res = pd_build_request(port, &rdo, &curr_limit, &supply_voltage,
			charging && max_request_allowed ?
			PD_REQUEST_MAX : PD_REQUEST_VSAFE5V);

	if (res != EC_SUCCESS)
		/*
		 * If fail to choose voltage, do nothing, let source re-send
		 * source cap
		 */
		return -1;

	if (!always_send_request) {
		/* Don't re-request the same voltage */
		if (pd[port].prev_request_mv == supply_voltage)
			return EC_SUCCESS;
#ifdef CONFIG_CHARGE_MANAGER
		/* Limit current to PD_MIN_MA during transition */
		else
			charge_manager_force_ceil(port, PD_MIN_MA);
#endif
	}

	CPRINTF("Req C%d [%d] %dmV %dmA", port, RDO_POS(rdo),
		supply_voltage, curr_limit);
	if (rdo & RDO_CAP_MISMATCH)
		CPRINTF(" Mismatch");
	CPRINTF("\n");

	pd[port].curr_limit = curr_limit;
	pd[port].supply_voltage = supply_voltage;
	pd[port].prev_request_mv = supply_voltage;
	res = send_request(port, rdo);

	if (res < 0)
		return res;

	return EC_SUCCESS;
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static int hc_remote_pd_discovery(struct host_cmd_handler_args *args)
{
	const uint8_t *port = args->params;
	struct ec_params_usb_pd_discovery_entry *r = args->response;

	if (*port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	r->vid = pd_get_identity_vid(*port);
	r->ptype = PD_IDH_PTYPE(pe[*port].identity[0]);
	/* pid only included if vid is assigned */
	if (r->vid)
		r->pid = PD_PRODUCT_PID(pe[*port].identity[2]);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_DISCOVERY,
			hc_remote_pd_discovery,
			EC_VER_MASK(0));
#endif

static void pe_src_startup(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Reset CapsCounter */
		pd[port].caps_counter = 0;
		/* Start SwapSourceStartTimer (Only after Swap) */
		if (pe_src[port].sm_flags & SM_FLAGS_POWER_ROLE_SWAP)
			pd[port].swap_source_start_timer = get_time().val +
						PD_T_SWAP_SOURCE_START;
		break;
	case RUN_SIG:
		if (pe_src[port].sm_flags & SM_FLAGS_POWER_ROLE_SWAP) {
			if (get_time().val > pd[port].swap_source_start_timer)
				set_state(port, SRC_OBJ(port),
						pe_src_send_capabilities);
		} else {
			set_state(port, SRC_OBJ(port),
						pe_src_send_capabilities);
		}
		break;
	}
}

static void pe_src_discovery(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		pd[port].source_capability_timer = get_time().val +
							PD_T_SEND_SOURCE_CAP;
		break;
	case RUN_SIG:
		if (get_time().val > pd[port].source_capability_timer) {
			if (pd[port].caps_counter <= N_CAPS_COUNT)
				set_state(port, SRC_OBJ(port),
						pe_src_send_capabilities);
			/*
			 * CapsCounter > nCapsCount
			 */
			else
				/* Disable Power Delivery */
				tc_disable_pd(port);
		}
		break;
	}
}

static void pe_src_send_capabilities(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Send PD Capabilities message */
		send_source_cap(port);
		/* Increment CapsCounter */
		pd[port].caps_counter++;
		pd[port].no_response_timer = get_time().val + PD_T_NO_RESPONSE;
		pd[port].sender_response_timer = 0;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE &&
					pd[port].sender_response_timer == 0) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;

			pd[port].hard_reset_counter = 0;
			pd[port].caps_counter = 0;
			pd[port].no_response_timer = 0;
			pd[port].sender_response_timer = get_time().val +
				PD_T_SENDER_RESPONSE;
		}
		/*
		 * Capabilities message sending failure
		 */
		else if (pd[port].sm_flags & SM_FLAGS_TX_ERROR)
			set_state(port, SRC_OBJ(port), pe_src_discovery);
		/*
		 * NoResponseTimer timeout & HardResetCounter > nHardResetcount
		 */
		else if (pd[port].no_response_timer > 0 &&
			 pd[port].hard_reset_counter > N_HARD_RESET_COUNT &&
			 get_time().val > pd[port].no_response_timer) {
			/* Disable Power Delivery */
			tc_disable_pd(port);
		} else if (pd[port].sender_response_timer > 0) {
			if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
				pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
				if (PD_HEADER_CNT(emsg[port].header) > 0) {
					/*
					 * Request Message Received
					 */
					if (PD_HEADER_TYPE(emsg[port].header) ==
							      PD_DATA_REQUEST) {
						set_state(port, SRC_OBJ(port),
						   pe_src_negotiate_capability);
						break;
					}
				}
				/* Non Request Message Received */
				/* Protocol Error ??? */
			}
			/*
			 * SenderResponseTimer timeout
			 */
			else if (get_time().val >
					    pd[port].sender_response_timer) {
				set_state(port, SRC_OBJ(port),
							pe_src_hard_reset);
			}
		}
		break;
	}
}

static void pe_src_negotiate_capability(int port, int sig)
{
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);

	switch (sig) {
	case ENTRY_SIG:
		if (!pd_check_requested_voltage(payload, port)) {
			pd[port].requested_idx = RDO_POS(payload);
			set_state(port, SRC_OBJ(port),
						pe_src_transition_supply);

		} else
			set_state(port, SRC_OBJ(port),
						pe_src_capability_response);
		break;
	case RUN_SIG:
		break;
	}
}

static void pe_src_transition_supply(int port, int sig)
{
	static int ps_rdy_state;

	switch (sig) {
	case ENTRY_SIG:
		/* explicit contract is now in place */
		pd[port].flags |= PD_FLAGS_EXPLICIT_CONTRACT;
		/* Transition Power Supply */
		pd_transition_voltage(pd[port].requested_idx);
		/* Send Accept message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
		ps_rdy_state = 0;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;
			switch (ps_rdy_state) {
			case 0:
				/* Send PS_RDY message */
				prl_send_ctrl_msg(port, TCPC_TX_SOP,
							PD_CTRL_PS_RDY);
				ps_rdy_state++;
				break;
			case 1:
				set_state(port, SRC_OBJ(port), pe_src_ready);
				break;
			}
		}
		/*
		 * Transimission Error
		 */
		else if (pd[port].sm_flags & SM_FLAGS_TX_ERROR)
			set_state(port, SRC_OBJ(port), pe_src_hard_reset);
		break;
	}
}

static void pe_src_ready(int port, int sig)
{
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	switch (sig) {
	case ENTRY_SIG:
		prl_end_ams(port);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;

			type = PD_HEADER_TYPE(emsg[port].header);
			cnt = PD_HEADER_CNT(emsg[port].header);
			ext = PD_HEADER_EXT(emsg[port].header);

			/* Extended Message Request */
			if (ext > 0) {
				switch (type) {
				case PD_EXT_GET_BATTERY_CAP:
					set_state(port, SNK_OBJ(port),
							pe_give_battery_cap);
					break;
				case PD_EXT_GET_BATTERY_STATUS:
					set_state(port, SNK_OBJ(port),
							pe_give_battery_status);
					break;
				default:
					set_state(port, SRC_OBJ(port),
						pe_send_not_supported);
				}
			}
			/* Data Messages */
			else if (cnt > 0) {
				switch (type) {
				case PD_DATA_REQUEST:
					set_state(port, SRC_OBJ(port),
						pe_src_negotiate_capability);
					break;
				case PD_DATA_BIST:
					break;
				case PD_DATA_SINK_CAP:
					break;
				case PD_DATA_VENDOR_DEF:
					break;
				default:
					set_state(port, SRC_OBJ(port),
						pe_send_not_supported);
				}
			}
			/* Control Messages */
			else {
				switch (type) {
				case PD_CTRL_GOOD_CRC:
					break;
				case PD_CTRL_PING:
					break;
				case PD_CTRL_GET_SINK_CAP:
					break;
				case PD_CTRL_GOTO_MIN:
					break;
				case PD_CTRL_PR_SWAP:
					break;
				case PD_CTRL_DR_SWAP:
					break;
				case PD_CTRL_VCONN_SWAP:
					break;
				case PD_CTRL_NOT_SUPPORTED:
					break;
				default:
					set_state(port, SRC_OBJ(port),
						pe_send_not_supported);
				}
			}
		}
		break;
	case EXIT_SIG:
		prl_start_ams(port);
		break;
	}
}

static void pe_src_capability_response(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Send Reject message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;

			if (pd[port].flags & PD_FLAGS_EXPLICIT_CONTRACT) {
				/*
				 * Explicit Contract &
				 * Reject message sent &
				 * Contract Invalid
				 */
				if (pd[port].sm_flags &
						SM_FLAGS_CONTRACT_INVALID)
					set_state(port, SRC_OBJ(port),
							pe_src_hard_reset);
				/*
				 * Explicit Contract &
				 * Reject message sent &
				 * Contract still valid
				 */
				else
					set_state(port, SRC_OBJ(port),
								pe_src_ready);
			}
			/*
			 * No Explicic Contract & Reject message sent
			 */
			else {
				set_state(port, SRC_OBJ(port),
						pe_src_wait_new_capabilities);
			}
		}
		break;
	}
}

static void pe_src_hard_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Generate Hard Reset Signal */
		pd_execute_hard_reset(port);
		/* Increment the HardResetCounter */
		pd[port].hard_reset_counter++;
		/* Start PSHardResetTimer */
		pd[port].ps_hard_reset_timer = get_time().val +
							PD_T_PS_HARD_RESET;
		break;
	case RUN_SIG:
		if (get_time().val > pd[port].ps_hard_reset_timer)
			set_state(port, SRC_OBJ(port),
						pe_src_transition_to_default);
		break;
	}
}

static void pe_src_hard_reset_received(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		pd[port].ps_hard_reset_timer = get_time().val +
							PD_T_PS_HARD_RESET;
		break;
	case RUN_SIG:
		if (get_time().val > pd[port].ps_hard_reset_timer)
			set_state(port, SRC_OBJ(port),
						pe_src_transition_to_default);
		break;
	}
}

static void pe_src_transition_to_default(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		pd_power_supply_reset(port);
		//tc_set_vconn(port, 0);
		tc_hard_reset(port);
		break;
	case RUN_SIG:
		set_state(port, SRC_OBJ(port), pe_src_startup);
		break;
	case EXIT_SIG:
//		tc_set_vconn(port, 1);
		prl_hard_reset_complete(port);
	break;
	}
}

static void pe_src_wait_new_capabilities(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* No new capabilities. Just stay here or disable pd??? */
		break;
	case RUN_SIG:
		break;
	}
}

/* Source Port Soft Reset States */
static void pe_src_send_soft_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Reset Protocol Layer */
		prl_init(port);
		/* Send Soft Reset Message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_SOFT_RESET);
		/* Initialize and run SenderResponseTimer */
		pd[port].sender_response_timer = get_time().val +
						PD_T_SENDER_RESPONSE;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			/*
			 * Accept Message received
			 */
			if ((PD_HEADER_CNT(emsg[port].header) == 0) &&
				(PD_HEADER_TYPE(emsg[port].header) ==
							PD_CTRL_ACCEPT))
				set_state(port, SRC_OBJ(port),
						pe_src_send_capabilities);
		}
		/*
		 * SenderResponseTimer timeout or Transmission Error
		 */
		else if ((get_time().val > pd[port].sender_response_timer) ||
				(pd[port].sm_flags & SM_FLAGS_TX_ERROR)) {
			set_state(port, SRC_OBJ(port), pe_src_hard_reset);
		}
		break;
	}
}

static void pe_src_soft_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Reset Protocol Layer */
		prl_init(port);
		/* Send Accpet Message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;

			set_state(port, SRC_OBJ(port),
					pe_src_send_capabilities);
		}
		/*
		 * Transimission Error
		 */
		else if (pd[port].sm_flags & SM_FLAGS_TX_ERROR)
			set_state(port, SRC_OBJ(port), pe_src_hard_reset);
	}
}
#if 0
/* Source Port Ping */
static void pe_src_ping(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Source Alert */
static void pe_src_send_source_alert(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Sink Alert */
static void pe_src_sink_alert_received(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Give Source Status */
static void pe_src_give_source_status(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Get Sink Status */
static void pe_src_get_sink_status(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Give Source Capabilities Extended */
static void pe_src_give_source_cap_ext(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Port Give PPS Status */
static void pe_src_give_pps_status(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}
#endif

/*
 * Sink Port States Machine
 */
static void pe_snk_startup(int port, int sig)
{
	switch (sig) {
	case RUN_SIG:
		/* Wait for VBUS */
		if (pd_snk_is_vbus_provided(port)) {
			set_state(port, SNK_OBJ(port),
				pe_snk_wait_for_capabilities);
		}
		break;
	}
}

static void pe_snk_wait_for_capabilities(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Initialize and run SinkWaitCapTimer */
		pe_snk[port].timeout = get_time().val + PD_T_SINK_WAIT_CAP;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			/*
			 * Source capabilities message received
			 */

			if ((PD_HEADER_CNT(emsg[port].header) > 0) &&
				   (PD_HEADER_TYPE(emsg[port].header) ==
				   PD_DATA_SOURCE_CAP)) {
				pd[port].sm_flags |= SM_FLAGS_PREVIOUS_PD_CONN;
				pe_snk_evaluate_capability(port);
				set_state(port, SNK_OBJ(port),
						pe_snk_select_capability);
			}
		}
		/*
		 * SinkWaitCapTimer timeout
		 */
		else if (get_time().val > pe_snk[port].timeout)
			set_state(port, SNK_OBJ(port), pe_snk_hard_reset);
		break;
	}
}

static void pe_snk_evaluate_capability(int port)
{
	uint32_t *pdo;
	uint32_t num;
	int i;

	/* Only adjust sink rev if source rev is higher.*/
	if (PD_HEADER_REV(emsg[port].header) < pd[port].rev)
		pd[port].rev = PD_HEADER_REV(emsg[port].header);

	num = emsg[port].len >> 2;
	pdo = (uint32_t *)emsg[port].buf;
	pd_src_cap_cnt[port] = num;

	for (i = 0; i < num; i++)
		pd_src_caps[port][i] = *pdo++;

	/* src cap 0 should be fixed PDO */
	pd_update_pdo_flags(port, pdo[0]);

	/* Evaluate the options based on supplied capabilities */
	pd_process_source_cap(port, pd_src_cap_cnt[port],
					pd_src_caps[port]);
}

static void pe_snk_select_capability(int port, int sig)
{
	uint8_t type;
	uint8_t cnt;

	switch (sig) {
	case ENTRY_SIG:
		/* Send Request */
		pd_send_request_msg(port, 1);

		/* Initialize and run SenderResponseTimer */
		pe_snk[port].timeout = get_time().val + PD_T_SENDER_RESPONSE;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;

			type = PD_HEADER_TYPE(emsg[port].header);
			cnt = PD_HEADER_CNT(emsg[port].header);

			if (cnt == 0) {
				/*
				 * Accept Message Received
				 */
				if (type == PD_CTRL_ACCEPT) {
					/* explicit contract is now in place */
					pd[port].flags |=
						PD_FLAGS_EXPLICIT_CONTRACT;
					set_state(port, SNK_OBJ(port),
						pe_snk_transition_sink);
				}
				/*
				 * Reject or Wait Message Received
				 */
				else if (type == PD_CTRL_REJECT ||
							type == PD_CTRL_WAIT) {
					if (type == PD_CTRL_WAIT)
						pd[port].sm_flags |=
								SM_FLAGS_WAIT;

					/* We have explicit contract */
					if (pd[port].flags &
						    PD_FLAGS_EXPLICIT_CONTRACT)
						set_state(port, SNK_OBJ(port),
							pe_snk_ready);
					/* No explicit contract */
					else
						set_state(port, SNK_OBJ(port),
						  pe_snk_wait_for_capabilities);
				}
			}
		}
		/*
		 * SenderResponsetimer timeout
		 */
		else if ((get_time().val > pe_snk[port].timeout) &&
					(pd[port].hard_reset_counter <=
					N_HARD_RESET_COUNT)) {
			set_state(port, SNK_OBJ(port), pe_snk_hard_reset);
		}
		break;
	}
}

static void pe_snk_transition_sink(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Initialize and run PSTransitionTimer */
		pe_snk[port].timeout = get_time().val + PD_T_PS_TRANSITION;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			/*
			 * PS_RDY message received
			 */
			if ((PD_HEADER_CNT(emsg[port].header) == 0) &&
				   (PD_HEADER_TYPE(emsg[port].header) ==
				   PD_CTRL_PS_RDY)) {
				set_state(port, SNK_OBJ(port), pe_snk_ready);
			}
		}
		/*
		 * Protocol Error
		 */
		else if ((get_time().val > pe_snk[port].timeout) &&
				  (pd[port].hard_reset_counter <=
				  N_HARD_RESET_COUNT)) {
			set_state(port, SNK_OBJ(port), pe_snk_hard_reset);
		}
		break;
	case EXIT_SIG:
		/* Transition Sink's power supply to the new power level */
		pd_set_input_current_limit(port, pd[port].curr_limit,
						pd[port].supply_voltage);
#ifdef CONFIG_CHARGE_MANAGER
		/* Set ceiling based on what's negotiated */
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
						pd[port].curr_limit);
#endif
		break;
	}
}

static void pe_snk_ready(int port, int sig)
{
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	switch (sig) {
	case ENTRY_SIG:
		prl_end_ams(port);
		if (pd[port].sm_flags & SM_FLAGS_WAIT)
			pe_snk[port].timeout = get_time().val +
							PD_T_SINK_REQUEST;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			type = PD_HEADER_TYPE(emsg[port].header);
			cnt = PD_HEADER_CNT(emsg[port].header);
			ext = PD_HEADER_EXT(emsg[port].header);
			/* Extended Message Request */
			if (ext > 0) {
				switch (type) {
				case PD_EXT_GET_BATTERY_CAP:
					set_state(port, SNK_OBJ(port),
							pe_give_battery_cap);
					break;
				case PD_EXT_GET_BATTERY_STATUS:
					set_state(port, SNK_OBJ(port),
							pe_give_battery_status);
					break;
				default:
					set_state(port, SNK_OBJ(port),
						pe_send_not_supported);
				}
			}
			/* Data Messages */
			else if (cnt > 0) {
				switch (type) {
				case PD_DATA_SOURCE_CAP:
					pe_snk_evaluate_capability(port);
					set_state(port, SNK_OBJ(port),
						pe_snk_select_capability);
					break;
				case PD_DATA_REQUEST:
					break;
				case PD_DATA_BIST:
					break;
				case PD_DATA_SINK_CAP:
					break;
				case PD_DATA_VENDOR_DEF:
					break;
				default:
					set_state(port, SNK_OBJ(port),
						pe_send_not_supported);
				}
			}
			/* Control Messages */
			else {
				switch (type) {
				case PD_CTRL_GOOD_CRC:
					break;
				case PD_CTRL_PING:
					break;
				case PD_CTRL_GET_SINK_CAP:
					set_state(port, SNK_OBJ(port),
						pe_snk_give_sink_cap);
					break;
				case PD_CTRL_GOTO_MIN:
					set_state(port, SNK_OBJ(port),
						pe_snk_transition_sink);
					break;
				case PD_CTRL_PR_SWAP:
					break;
				case PD_CTRL_DR_SWAP:
					break;
				case PD_CTRL_VCONN_SWAP:
					break;
				case PD_CTRL_NOT_SUPPORTED:
					break;
				default:
					set_state(port, SNK_OBJ(port),
						pe_send_not_supported);
				}
			}
		} else if (pd[port].sm_flags & SM_FLAGS_WAIT) {
			pd[port].sm_flags &= ~SM_FLAGS_WAIT;
			/*
			 * SinkRequestTimer timeout
			 */
			if (get_time().val > pe_snk[port].timeout)
				set_state(port, SNK_OBJ(port),
						pe_snk_select_capability);
		}
		break;
	case EXIT_SIG:
		prl_start_ams(port);
		break;
	}
}

static void pe_snk_hard_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		pd_execute_hard_reset(port);
		pd[port].hard_reset_counter++;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_HARD_RESET_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_HARD_RESET_COMPLETE;

			set_state(port, SNK_OBJ(port),
					pe_snk_transition_to_default);
		}
		break;
	}
}

static void pe_snk_transition_to_default(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		tc_hard_reset(port);
		break;
	case RUN_SIG:
		set_state(port, SNK_OBJ(port), pe_snk_startup);
		break;
	case EXIT_SIG:
		prl_hard_reset_complete(port);
		break;
	}
}

static void pe_snk_give_sink_cap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Send a Sink_Capabilities Message */
		emsg[port].len = pd_snk_pdo_cnt * 4;
		memcpy(emsg[port].buf, (uint8_t *)pd_snk_pdo, emsg[port].len);
		prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_SINK_CAP);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;

			set_state(port, SNK_OBJ(port), pe_snk_ready);
		}
		break;
	}
}

/*
 * Sink Port Soft Reset and Protocol Error State Machine
 */
static void pe_snk_send_soft_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Reset Protocol Layer */
		prl_init(port);
		/* Send Soft Reset Message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_SOFT_RESET);
		/* Initialize and run SenderResponseTimer */
		pd[port].sender_response_timer = get_time().val +
						PD_T_SENDER_RESPONSE;
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;

			/*
			 * Accept Message received
			 */
			if ((PD_HEADER_CNT(emsg[port].header) == 0) &&
				   (PD_HEADER_TYPE(emsg[port].header) ==
				   PD_CTRL_ACCEPT))
				set_state(port, SNK_OBJ(port),
						pe_snk_wait_for_capabilities);
		}
		/*
		 * SenderResponseTimer timeout or Transmission Error
		 */
		else if ((get_time().val > pd[port].sender_response_timer) ||
				(pd[port].sm_flags & SM_FLAGS_TX_ERROR)) {
			set_state(port, SNK_OBJ(port), pe_snk_hard_reset);
		}
		break;
	}
}

static void pe_snk_soft_reset(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		/* Reset Protocol Layer */
		prl_init(port);
		/* Send Accpet Message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_TX_COMPLETE) {
			pd[port].sm_flags &= ~SM_FLAGS_TX_COMPLETE;

			set_state(port, SNK_OBJ(port),
					pe_snk_wait_for_capabilities);
		}
		/*
		 * Transimission Error
		 */
		else if (pd[port].sm_flags & SM_FLAGS_TX_ERROR)
			set_state(port, SNK_OBJ(port), pe_snk_hard_reset);
		break;
	}
}

static void pe_send_not_supported(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_SOFT_RESET);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			if (tc_get_power_role(port) == PD_ROLE_SOURCE)
				set_state(port, SRC_OBJ(port), pe_src_ready);
			else
				set_state(port, SNK_OBJ(port), pe_snk_ready);
		}
	}
}

/* Give Battery Capabilities */
static void pe_give_battery_cap(int port, int sig)
{
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);
	uint16_t *msg = (uint16_t *)&emsg[port].buf;

	switch (sig) {
	case ENTRY_SIG:
		emsg[port].len = 12;

		/* Set extended header */
		msg[0] = PD_EXT_HEADER(0, /* Chunk Number */
				       0, /* Request Chunk */
				       9  /* Data Size in bytes */
				      );
		/* Set VID */
		msg[1] = USB_VID_GOOGLE;

		/* Set PID */
		msg[2] = 0; /* FIXME: This should be defined CONFIG_USB_PID; */
		msg[3] = 0;
		msg[4] = 0;
		msg[5] = 0;

		if (battery_is_present()) {
			/*
			 * We only have one fixed battery,
			 * so make sure batt cap ref is 0.
			 */
			if (BATT_CAP_REF(payload) != 0) {
				/* Invalid battery reference */
				msg[5] = 1;
			} else {
				uint32_t v;
				uint32_t c;

				/*
				 * The Battery Design Capacity field shall
				 * return the Battery’s design capacity in
				 * tenths of Wh. If the Battery is Hot Swappable
				 * and is not present, the Battery Design
				 * Capacity field shall be set to 0. If the
				 * Battery is unable to report its Design
				 * Capacity, it shall return 0xFFFF
				 */
				msg[3] = 0xffff;

				/*
				 * The Battery Last Full Charge Capacity field
				 * shall return the Battery’s last full charge
				 * capacity in tenths of Wh. If the Battery is
				 * Hot Swappable and is not present, the Battery
				 * Last Full Charge Capacity field shall be set
				 * to 0. If the Battery is unable to report its
				 * Design Capacity, the Battery Last Full Charge
				 * Capacity field shall be set to 0xFFFF.
				 */
				 msg[4] = 0xffff;

				if (battery_design_voltage(&v) == 0) {
					if (battery_design_capacity(&c) == 0) {
						/*
						 * Wh = (c * v) / 1000000
						 * 10th of a Wh = Wh * 10
						 */
						msg[3] =
						    DIV_ROUND_NEAREST((c * v),
									100000);
					}

					if (battery_full_charge_capacity(&c) ==
									    0) {
						/*
						 * Wh = (c * v) / 1000000
						 * 10th of a Wh = Wh * 10
						 */
						msg[4] =
						    DIV_ROUND_NEAREST((c * v),
									100000);
					}
				}
			}
		}

		prl_send_ext_data_msg(port, TCPC_TX_SOP, PD_EXT_BATTERY_CAP);

		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			if (tc_get_power_role(port) == PD_ROLE_SOURCE)
				set_state(port, SRC_OBJ(port), pe_src_ready);
			else
				set_state(port, SNK_OBJ(port), pe_snk_ready);
		}
		break;
	}
}

/* Give Battery Status */
static void pe_give_battery_status(int port, int sig)
{
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);
	uint32_t *msg = (uint32_t *)&emsg[port].buf;

	switch (sig) {
	case ENTRY_SIG:
		emsg[port].len = 4;
		*msg = 0;

		if (battery_is_present()) {
			/*
			 * We only have one fixed battery,
			 * so make sure batt cap ref is 0.
			 */
			if (BATT_CAP_REF(payload) != 0) {
				/* Invalid battery reference */
				*msg |= BSDO_INVALID;
			} else {
				uint32_t v;
				uint32_t c;

				if (battery_design_voltage(&v) != 0 ||
					battery_remaining_capacity(&c) != 0) {
					*msg |= BSDO_CAP(BSDO_CAP_UNKNOWN);
				} else {
					/*
					 * Wh = (c * v) / 1000000
					 * 10th of a Wh = Wh * 10
					 */
					*msg |=
					    BSDO_CAP(DIV_ROUND_NEAREST((c * v),
								      100000));
				}

				/* Battery is present */
				*msg |= BSDO_PRESENT;

				/*
				 * For drivers that are not smart battery
				 * compliant, battery_status() returns
				 * EC_ERROR_UNIMPLEMENTED and the battery is
				 * assumed to be idle.
				 */
				if (battery_status(&c) != 0) {
					*msg |= BSDO_IDLE; /* assume idle */
				} else {
					if (c & STATUS_FULLY_CHARGED)
						/* Fully charged */
						*msg |= BSDO_IDLE;
					else if (c & STATUS_DISCHARGING)
						/* Discharging */
						*msg |= BSDO_DISCHARGING;
					/* else battery is charging.*/
				}
			}
		} else {
			*msg = BSDO_CAP(BSDO_CAP_UNKNOWN);
		}

		prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_BATTERY_STATUS);
		break;
	case RUN_SIG:
		if (pd[port].sm_flags & SM_FLAGS_MSG_RECEIVED) {
			pd[port].sm_flags &= ~SM_FLAGS_MSG_RECEIVED;
			if (tc_get_power_role(port) == PD_ROLE_SOURCE)
				set_state(port, SRC_OBJ(port), pe_src_ready);
			else
				set_state(port, SNK_OBJ(port), pe_snk_ready);
		}
		break;
	}
}

#if 0
/* DFP to UFP Data Role Swap */
static void pe_drs_dfp_ufp_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_dfp_ufp_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_dfp_ufp_change_to_ufp(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_dfp_ufp_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_dfp_ufp_reject_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* UFP to DFP Data Role Swap */
static void pe_drs_ufp_dfp_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_ufp_dfp_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_ufp_dfp_change_to_dfp(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_ufp_dfp_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_drs_ufp_dfp_reject_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source to Sink Power Role Swap */
static void pe_prs_src_snk_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_transition_to_off(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_assert_rd(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_wait_source_on(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_src_snk_reject_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Sink to Source Power Role Swap */
static void pe_prs_snk_src_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_transition_to_off(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_assert_rp(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_source_on(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_prs_snk_src_reject_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source to Sink Fast Role Swap */
static void pe_frs_src_snk_cc_signal(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_src_snk_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_src_snk_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_src_snk_transition_to_off(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_src_snk_assert_rd(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_src_snk_wait_source_on(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Sink to Source Fast Role Swap */
static void pe_frs_snk_src_start_ams(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_snk_src_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_snk_src_transition_to_off(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_snk_src_vbus_applied(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_snk_src_assert_rp(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_frs_snk_src_source_on(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Source Port Get Source Capabilities */
static void pe_dr_src_get_source_cap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Source Port Give Sink Capabilities */
static void pe_dr_src_give_sink_cap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Sink Port Get Sink Capabilities */
static void pe_dr_snk_get_sink_cap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Sink Port Give Source Capabilities */
static void pe_dr_snk_give_source_cap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Source Port Get Source Capabilities Extended */
static void pe_dr_src_get_source_cap_ext(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Dual-Role Sink Port Give Source Capabilities Extended */
static void pe_dr_snk_give_source_cap_ext(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* USB Type-C V CONN Swap */
static void pe_vcs_send_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_evaluate_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_accept_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_reject_swap(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_wait_for_vconn(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_turn_off_vconn(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_turn_on_vconn(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_vcs_send_ps_rdy(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Initiator to Port Structured VDM Discover Identity */
static void pe_init_port_vdm_identity_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_port_vdm_identity_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_port_vdm_identity_naked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Initiator Structured VDM Discover SVIDs */
static void pe_init_vdm_svids_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_vdm_svids_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_vdm_svids_naked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Initiator Structured VDM Discover Modes */
static void pe_init_vdm_modes_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_vdm_modes_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_init_vdm_modes_naked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Initiator Structured VDM Attention */
static void pe_init_vdm_attention_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Responder Structured VDM Discovery Identity */
static void pe_resp_vdm_get_identity(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_resp_vdm_send_identity(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}
static void pe_resp_vdm_get_identity_nak(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Responder Structured VDM Discovery SVIDs */
static void pe_resp_vdm_get_svids(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_resp_vdm_send_svids(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_resp_vdm_get_svids_nak(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Responder Structured VDM Discovery Modes */
static void pe_resp_vdm_get_modes(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_resp_vdm_send_modes(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_resp_vdm_get_modes_nak(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Receiving a Structured VDM Attention*/
static void pe_rcv_vdm_attention_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* DFP Structured VDM Mode Entry */
static void pe_dfp_vdm_mode_entry_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_dfp_vdm_mode_entry_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_dfp_vdm_mode_entry_naked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* DFP Structured VDM Mode Exit */
static void pe_dfp_vdm_mode_exit_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_dfp_vdm_mode_exit_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* UFP Structured VDM Enter Mode */
static void pe_ufp_vdm_evaluate_mode_entry(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_ufp_vdm_mode_entry_ack(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_ufp_vdm_mode_entry_nak(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/*  UFP Structured VDM Exit Mode */
static void pe_ufp_vdm_mode_exit(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_ufp_vdm_mode_exit_ack(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
	break;
	}
}

static void pe_ufp_vdm_mode_exit_nak(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* Source Startup Structured VDM Discover Identity */
static void pe_src_vdm_identity_request(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_src_vdm_identity_acked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

static void pe_src_vdm_identity_naked(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

/* BIST Carrier Mode */
static void pe_bist_carrier_mode(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}

#endif
