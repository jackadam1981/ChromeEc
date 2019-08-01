/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "atomic.h"
#include "battery.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "task.h"
#include "tcpm.h"
#include "util.h"
#include "usb_pd.h"
#include "usb_pd_tcpm.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_tc_sm.h"
#include "usb_emsg.h"
#include "usb_sm.h"
#include "usbc_ppc.h"

/*
 * USB Policy Engine Sink / Source module
 *
 * Based on Revision 3.0, Version 1.2 of
 * the USB Power Delivery Specification.
 */

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)
#endif

#ifdef CONFIG_USB_PD_DEBUG_LEVEL
#define DEBUG_PRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#else
#define DEBUG_PRINTF(format, args...)
#endif


#define PE_SET_FLAG(port, flag) atomic_or(&pe[port].flags, (flag))
#define PE_CLR_FLAG(port, flag) atomic_clear(&pe[port].flags, (flag))
#define PE_CHK_FLAG(port, flag) (pe[port].flags & (flag))

/*
 * PE_OBJ is a convenience macro to access struct sm_obj, which
 * must be the first member of struct policy_engine.
 */
#define PE_OBJ(port)   (SM_OBJ(pe[port]))

/*
 * These macros SET, CLEAR, and CHECK, a DPM (Device Policy Manager)
 * Request. The Requests are listed in usb_pe_sm.h.
 */
#define PE_SET_DPM_REQUEST(port, req) (pe[port].dpm_request |=  (req))
#define PE_CLR_DPM_REQUEST(port, req) (pe[port].dpm_request &= ~(req))
#define PE_CHK_DPM_REQUEST(port, req) (pe[port].dpm_request &   (req))

/* Policy Engine Layer Flags */
#define PE_FLAGS_PD_CONNECTION                  BIT(0)
#define PE_FLAGS_ACCEPT                         BIT(1)
#define PE_FLAGS_PS_READY                       BIT(2)
#define PE_FLAGS_PROTOCOL_ERROR                 BIT(3)
#define PE_FLAGS_MODAL_OPERATION                BIT(4)
#define PE_FLAGS_TX_COMPLETE                    BIT(5)
#define PE_FLAGS_MSG_RECEIVED                   BIT(6)
#define PE_FLAGS_HARD_RESET_PENDING             BIT(7)
#define PE_FLAGS_WAIT                           BIT(8)
#define PE_FLAGS_EXPLICIT_CONTRACT              BIT(9)
#define PE_FLAGS_SNK_WAIT_CAP_TIMEOUT           BIT(10)
#define PE_FLAGS_PS_TRANSITION_TIMEOUT          BIT(11)
#define PE_FLAGS_INTERRUPTIBLE_AMS              BIT(12)
#define PE_FLAGS_PS_RESET_COMPLETE              BIT(13)
#define PE_FLAGS_SEND_SVDM                      BIT(14)
#define PE_FLAGS_VCONN_SWAP_COMPLETE            BIT(15)
#define PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE    BIT(16)
#define PE_FLAGS_DISCOVER_VDM_IDENTITY_DONE     BIT(17)
#define PE_FLAGS_PRL_RESET_PENDING              BIT(19)
#define PE_FLAGS_RUN_SOURCE_START_TIMER         BIT(20)
#define PE_FLAGS_VDM_REQUEST_BUSY               BIT(21)
#define PE_FLAGS_VDM_REQUEST_NAKED              BIT(22)

#define PRL_TX_RX_SIGNAL (PE_FLAGS_TX_COMPLETE | PE_FLAGS_MSG_RECEIVED)

#define N_CAPS_COUNT 25
#define N_DISCOVER_IDENTITY_COUNT 2
#define N_HARD_RESET_COUNT 2

/*
 * NOTE:
 *	DO_PORT_DISCOVERY_START is not actually a vdm command. It is used
 *	to start the port partner discovery proccess.
 */
enum vdm_cmd {
	DO_PORT_DISCOVERY_START,
	DISCOVER_IDENTITY,
	DISCOVER_SVIDS,
	DISCOVER_MODES,
	ENTER_MODE,
	EXIT_MODE,
	ATTENTION,
};

enum port_partner {
	PORT,
	CABLE,
};

/*
 * This enum is used to implement a state machine consisting of at most
 * 3 states, inside a Policy Engine State.
 */
enum sub_state {
	PE_SUB0,
	PE_SUB1,
	PE_SUB2
};

static enum sm_local_state local_state[CONFIG_USB_PD_PORT_COUNT];

#define PE_OBJ(port)   (SM_OBJ(pe[port]))

/*
 * Policy Engine State Machine Object
 */
static struct policy_engine {
	/* struct sm_obj must be first. */
	struct sm_obj obj;
	/* state id */
	enum pe_states state_id;
	/* current port power role (SOURCE or SINK) */
	enum pd_power_role power_role;
	/* current port data role (DFP or UFP) */
	enum pd_data_role data_role;
	/* saved data and power roles while communicating with a cable plug */
	enum pd_data_role saved_data_role;
	enum pd_power_role saved_power_role;
	/* state machine flags */
	uint32_t flags;
	/* Device Policy Manager Request */
	uint32_t dpm_request;
	/* state timeout timer */
	uint64_t timeout;
	/* last requested voltage PDO index */
	int requested_idx;

	/* Current limit / voltage based on the last request message */
	uint32_t curr_limit;
	uint32_t supply_voltage;

	/* state specific state machine variable */
	enum sub_state sub;

	/* VDO */
	int32_t active_cable_vdo1;
	int32_t active_cable_vdo2;
	int32_t passive_cable_vdo;
	int32_t ama_vdo;
	int32_t vpd_vdo;
	/* alternate mode policy*/
	struct pd_policy am_policy;

	/* VDM */
	enum port_partner partner_is_cable;
	uint32_t vdm_cmd;
	uint32_t vdm_cnt;
	uint32_t vdm_data[VDO_HDR_SIZE + VDO_MAX_SIZE];

	/* Timers */

	/*
	 * The NoResponseTimer is used by the Policy Engine in a Source
	 * to determine that its Port Partner is not responding after a
	 * Hard Reset.
	 */
	uint64_t no_response_timer;

	/*
	 * Prior to a successful negotiation a Source Shall use the
	 * SourceCapabilityTimer to periodically send out a
	 * Source_Capabilities Message
	 */
	uint64_t source_cap_timer;
	uint64_t ps_transition_timer;
	uint64_t sender_response_timer;
	uint64_t discover_identity_timer;
	uint64_t ps_hard_reset_timer;
	uint64_t sink_request_timer;
	uint64_t ps_source_timer;
	uint64_t bist_cont_mode_timer;
	uint64_t swap_source_start_timer;
	uint64_t vdm_response_timer;
	uint64_t vconn_on_timer;

	/* Counters */

	uint32_t hard_reset_counter;
	uint32_t caps_counter;
	uint32_t port_discover_identity_count;
	uint32_t cable_discover_identity_count;

	/* Last received source cap */
	uint32_t src_caps[PDO_MAX_OBJECTS];
	int src_cap_cnt;

} pe[CONFIG_USB_PD_PORT_COUNT];

/*
 * As a sink, this is the max voltage (in millivolts) we can request
 * before getting source caps
 */
static unsigned int max_request_mv = PD_MAX_VOLTAGE_MV;

/*
 * Private VDM utility functions
 */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static int validate_mode_request(struct svdm_amode_data *modep,
						uint16_t svid, int opos);
static void dfp_consume_attention(int port, uint32_t *payload);
static void dfp_consume_identity(int port, int cnt, uint32_t *payload);
static void dfp_consume_svids(int port, int cnt, uint32_t *payload);
static int dfp_discover_modes(int port, uint32_t *payload);
static void dfp_consume_modes(int port, int cnt, uint32_t *payload);
static int get_mode_idx(int port, uint16_t svid);
static struct svdm_amode_data *get_modep(int port, uint16_t svid);
#endif

/*
 * Policy Engine States
 */

/* Source Port */
DECLARE_STATE(pe, src_startup, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_discovery, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_send_capabilities, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_negotiate_capability, NOOP, NOOP);
DECLARE_STATE(pe, src_transition_supply, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_ready, WITH_RUN, WITH_EXIT);
DECLARE_STATE(pe, src_disabled, NOOP, NOOP);
DECLARE_STATE(pe, src_capability_response, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_hard_reset, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_hard_reset_received, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_transition_to_default, WITH_RUN, NOOP);
DECLARE_STATE(pe, src_vdm_identity_request, WITH_RUN, NOOP);

/* Sink Port */
DECLARE_STATE(pe, snk_startup, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_discovery, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_wait_for_capabilities, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_evaluate_capability, NOOP, NOOP);
DECLARE_STATE(pe, snk_select_capability, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_ready, WITH_RUN, WITH_EXIT);
DECLARE_STATE(pe, snk_hard_reset, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_transition_to_default, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_give_sink_cap, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_get_source_cap, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_give_source_cap, WITH_RUN, NOOP);
DECLARE_STATE(pe, snk_transition_sink, WITH_RUN, WITH_EXIT);

/* Soft Reset */
DECLARE_STATE(pe, send_soft_reset, WITH_RUN, WITH_EXIT);
DECLARE_STATE(pe, soft_reset, WITH_RUN, NOOP);

/* Not Supported Message */
DECLARE_STATE(pe, send_not_supported, WITH_RUN, NOOP);

/* PE_SRC_Ping Message */
DECLARE_STATE(pe, src_ping, WITH_RUN, NOOP);

/* PE_Give_Battery_Cap Message */
DECLARE_STATE(pe, give_battery_cap, WITH_RUN, NOOP);

/* PE_Give_Battery_Status Message */
DECLARE_STATE(pe, give_battery_status, WITH_RUN, NOOP);
DECLARE_STATE(pe, drs_evaluate_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, drs_change, WITH_RUN, NOOP);
DECLARE_STATE(pe, drs_send_swap, WITH_RUN, NOOP);

/* Power Role Swap from SRC to SNK */
DECLARE_STATE(pe, prs_src_snk_evaluate_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_src_snk_transition_to_off, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_src_snk_wait_source_on, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_src_snk_send_swap, WITH_RUN, WITH_EXIT);

/* Power role Swap from SNK to SRC */
DECLARE_STATE(pe, prs_snk_src_evaluate_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_snk_src_transition_to_off, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_snk_src_assert_rp, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_snk_src_source_on, WITH_RUN, NOOP);
DECLARE_STATE(pe, prs_snk_src_send_swap, WITH_RUN, WITH_EXIT);

#ifdef CONFIG_USBC_VCONN
/* VCONN Swap */
DECLARE_STATE(pe, vcs_evaluate_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, vcs_send_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, vcs_wait_for_vconn_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, vcs_turn_on_vconn_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, vcs_turn_off_vconn_swap, WITH_RUN, NOOP);
DECLARE_STATE(pe, vcs_send_ps_rdy_swap, WITH_RUN, NOOP);
#endif

DECLARE_STATE(pe, do_port_discovery, WITH_RUN, NOOP);
DECLARE_STATE(pe, vdm_request, WITH_RUN, WITH_EXIT);
DECLARE_STATE(pe, vdm_acked, NOOP, NOOP);
DECLARE_STATE(pe, vdm_response, WITH_RUN, NOOP);
DECLARE_STATE(pe, handle_custom_vdm_request, WITH_RUN, NOOP);

DECLARE_STATE(pe, wait_for_error_recovery, WITH_RUN, WITH_EXIT);
DECLARE_STATE(pe, bist, WITH_RUN, WITH_EXIT);

void pe_init(int port)
{
	pe[port].flags = 0;
	pe[port].dpm_request = 0;
	pe[port].source_cap_timer = 0;
	pe[port].no_response_timer = 0;
	pe[port].data_role = tc_get_data_role(port);

	tc_pd_connection(port, 0);

	if (tc_get_power_role(port) == PD_ROLE_SOURCE)
		sm_init_state(port, PE_OBJ(port), pe_src_startup);
	else
		sm_init_state(port, PE_OBJ(port), pe_snk_startup);
}

int pe_is_running(int port)
{
	return local_state[port] == SM_RUN;
}

void usbc_policy_engine(int port, int evt, int en)
{
	switch (local_state[port]) {
	case SM_PAUSED:
		if (!en)
			break;
		else
			local_state[port] = SM_INIT;
			/* fall through */
	case SM_INIT:
		pe_init(port);
		local_state[port] = SM_RUN;
		/* fall through */
	case SM_RUN:
		if (!en) {
			local_state[port] = SM_PAUSED;
			break;
		}
		/* Run state machine */
		sm_run_state_machine(port, PE_OBJ(port), SM_RUN_SIG);
		break;
	}
}

enum pe_states pe_get_state_id(int port)
{
	return pe[port].state_id;
}

void pe_prl_reset_pending(int port)
{
	PE_SET_FLAG(port, PE_FLAGS_PRL_RESET_PENDING);
}

void pe_prl_reset_complete(int port)
{
	PE_CLR_FLAG(port, PE_FLAGS_PRL_RESET_PENDING);
}

int pe_is_explicit_contract(int port)
{
	return PE_CHK_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);
}

void pe_pass_up_message(int port)
{
	PE_SET_FLAG(port, PE_FLAGS_MSG_RECEIVED);
}

void pe_hard_reset_sent(int port)
{
	PE_CLR_FLAG(port, PE_FLAGS_HARD_RESET_PENDING);
}

void pe_got_hard_reset(int port)
{
	/*
	 * Transition from any state to the PE_SRC_Hard_Reset_Received or
	 *  PE_SNK_Transition_to_default state when:
	 *  1) Hard Reset Signaling is detected.
	 */
	pe[port].power_role = tc_get_power_role(port);

	if (pe[port].power_role == PD_ROLE_SOURCE)
		sm_set_state(port, PE_OBJ(port), pe_src_hard_reset_received);
	else
		sm_set_state(port, PE_OBJ(port), pe_snk_transition_to_default);
}

void pe_report_error(int port, enum pe_error e)
{
	/*
	 * Generate Hard Reset if Protocol Error occurred
	 * while in PE_Send_Soft_Reset state.
	 */
	if (pe[port].obj.task_state == pe_send_soft_reset) {
		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
		return;
	}

	if (pe[port].obj.task_state == pe_src_send_capabilities ||
			pe[port].obj.task_state == pe_src_transition_supply ||
			pe[port].obj.task_state ==
					pe_prs_src_snk_wait_source_on ||
			pe[port].obj.task_state ==
					pe_src_disabled ||
			pe[port].obj.task_state ==
					pe_src_discovery ||
			pe[port].obj.task_state ==
					pe_vdm_request) {
		PE_SET_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);
		return;
	}

	/*
	 * The PE_Send_Soft_Reset state shall be entered from
	 * any state when a Protocol Error is detected by
	 * Protocol Layer during a Non-Interruptible AMS or when
	 * Message has not been sent after retries.
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS) ||
					(e == ERR_TCH_XMIT)) {
		sm_set_state(port, PE_OBJ(port), pe_send_soft_reset);
	}
	/*
	 * Transition to PE_Snk_Ready or PE_Src_Ready by a Protocol
	 * Error during an Interruptible AMS.
	 */
	else {
		PE_SET_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);
		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
	}
}

void pe_got_soft_reset(int port)
{
	/*
	 * The PE_SRC_Soft_Reset state Shall be entered from any state when a
	 * Soft_Reset Message is received from the Protocol Layer.
	 */
	sm_set_state(port, PE_OBJ(port), pe_soft_reset);
}

void pe_dpm_request(int port, enum pe_dpm_request req)
{
	if (pe[port].state_id == PE_SRC_READY ||
			pe[port].state_id == PE_SNK_READY)
		PE_SET_DPM_REQUEST(port, req);
}

void pe_vconn_swap_complete(int port)
{
	PE_SET_FLAG(port, PE_FLAGS_VCONN_SWAP_COMPLETE);
}

void pe_ps_reset_complete(int port)
{
	PE_SET_FLAG(port, PE_FLAGS_PS_RESET_COMPLETE);
}

void pe_message_sent(int port)
{
	PE_SET_FLAG(port, PE_FLAGS_TX_COMPLETE);
}

void pe_send_vdm(int port, uint32_t vid, int cmd, const uint32_t *data,
						int count)
{
	pe[port].partner_is_cable = PORT;

	/* Copy VDM Header */
	pe[port].vdm_data[0] = VDO(vid, ((vid & USB_SID_PD) == USB_SID_PD) ?
				1 : (PD_VDO_CMD(cmd) <= CMD_ATTENTION),
				VDO_SVDM_VERS(1) | cmd);

	/* Copy Data after VDM Header */
	memcpy((pe[port].vdm_data + 1), data, count);

	pe[port].vdm_cnt = count + 1;

	PE_SET_FLAG(port, PE_FLAGS_SEND_SVDM);
	task_wake(PD_PORT_TO_TASK_ID(port));
}

#ifdef CONFIG_POWER_COMMON /* Needed b/c CONFIG_POWER_COMMON is only caller */
void pe_exit_dp_mode(int port)
{
	if (IS_ENABLED(CONFIG_USB_PD_ALT_MODE_DFP)) {
		int opos = pd_alt_mode(port, USB_SID_DISPLAYPORT);

		if (opos <= 0)
			return;

		CPRINTS("C%d Exiting DP mode", port);
		if (!pd_dfp_exit_mode(port, USB_SID_DISPLAYPORT, opos))
			return;

		pe_send_vdm(port, USB_SID_DISPLAYPORT,
				CMD_EXIT_MODE | VDO_OPOS(opos), NULL, 0);
	}
}
#endif /* CONFIG_POWER_COMMON */

/*
 * Private functions
 */

static void send_source_cap(int port)
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
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	}

	emsg[port].len = src_pdo_cnt * 4;
	memcpy(emsg[port].buf, (uint8_t *)src_pdo, emsg[port].len);

	prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_SOURCE_CAP);
}

/*
 * Request desired charge voltage from source.
 */
static void pe_send_request_msg(int port)
{
	uint32_t rdo;
	uint32_t curr_limit;
	uint32_t supply_voltage;
	int charging;
	int max_request_allowed;

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charging = (charge_manager_get_active_charge_port() == port);
	else
		charging = 1;

	if (IS_ENABLED(CONFIG_USB_PD_CHECK_MAX_REQUEST_ALLOWED))
		max_request_allowed = pd_is_max_request_allowed();
	else
		max_request_allowed = 1;

	/* Build and send request RDO */
	/*
	 * If this port is not actively charging or we are not allowed to
	 * request the max voltage, then select vSafe5V
	 */
	pd_build_request(port, &rdo, &curr_limit, &supply_voltage,
		charging && max_request_allowed ?
		PD_REQUEST_MAX : PD_REQUEST_VSAFE5V);

	CPRINTF("C%d Req [%d] %dmV %dmA", port, RDO_POS(rdo),
					supply_voltage, curr_limit);
	if (rdo & RDO_CAP_MISMATCH)
		CPRINTF(" Mismatch");
	CPRINTF("\n");

	pe[port].curr_limit = curr_limit;
	pe[port].supply_voltage = supply_voltage;

	emsg[port].len = 4;

	memcpy(emsg[port].buf, (uint8_t *)&rdo, emsg[port].len);
	prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_REQUEST);
}

static void pe_update_pdo_flags(int port, uint32_t pdo)
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

	if (pdo & PDO_FIXED_DUAL_ROLE)
		tc_partner_dr_power(port, 1);
	else
		tc_partner_dr_power(port, 0);

	if (pdo & PDO_FIXED_EXTERNAL)
		tc_partner_extpower(port, 1);
	else
		tc_partner_extpower(port, 0);

	if (pdo & PDO_FIXED_COMM_CAP)
		tc_partner_usb_comm(port, 1);
	else
		tc_partner_usb_comm(port, 0);

	if (pdo & PDO_FIXED_DATA_SWAP)
		tc_partner_dr_data(port, 1);
	else
		tc_partner_dr_data(port, 0);

#ifdef CONFIG_CHARGE_MANAGER
	/*
	 * Treat device as a dedicated charger (meaning we should charge
	 * from it) if it does not support power swap, or if it is externally
	 * powered, or if we are a sink and the device identity matches a
	 * charging white-list.
	 */
	if (!(pdo & PDO_FIXED_DUAL_ROLE) || (pdo & PDO_FIXED_EXTERNAL) ||
		charge_whitelisted)
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	else
		charge_manager_update_dualrole(port, CAP_DUALROLE);
#endif
}

int pd_board_check_request(uint32_t rdo, int pdo_cnt)
{
	int idx = RDO_POS(rdo);

	/* Check for invalid index */
	return (!idx || idx > pdo_cnt) ?
		EC_ERROR_INVAL : EC_SUCCESS;
}

static void pe_prl_execute_hard_reset(int port)
{
	prl_execute_hard_reset(port);
}

/**
 * PE_SRC_Startup
 */
static int pe_src_startup(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_startup_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_startup_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_STARTUP\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_STARTUP;

	/* Initialize VDOs to default values */
	pe[port].active_cable_vdo1 = -1;
	pe[port].active_cable_vdo2 = -1;
	pe[port].passive_cable_vdo = -1;
	pe[port].ama_vdo = -1;
	pe[port].vpd_vdo = -1;

	/* Reset CapsCounter */
	pe[port].caps_counter = 0;

	/* Reset the protocol layer */
	prl_reset(port);

	/* Set initial data role */
	pe[port].data_role = tc_get_data_role(port);

	/* Set initial power role */
	pe[port].power_role = PD_ROLE_SOURCE;

	/* Clear explicit contract. */
	PE_CLR_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);

	pe[port].cable_discover_identity_count = 0;
	pe[port].port_discover_identity_count = 0;

	if (PE_CHK_FLAG(port, PE_FLAGS_RUN_SOURCE_START_TIMER)) {
		PE_CLR_FLAG(port, PE_FLAGS_RUN_SOURCE_START_TIMER);
		/* Start SwapSourceStartTimer */
		pe[port].swap_source_start_timer =
			get_time().val + PD_T_SWAP_SOURCE_START;
	} else {
		pe[port].swap_source_start_timer = 0;
	}

	return 0;
}

static int pe_src_startup_run(int port)
{
	/* Wait until protocol layer is done resetting */
	if (PE_CHK_FLAG(port, PE_FLAGS_PRL_RESET_PENDING))
		return 0;

	if (pe[port].swap_source_start_timer == 0 ||
			get_time().val > pe[port].swap_source_start_timer)
		sm_set_state(port, PE_OBJ(port), pe_src_vdm_identity_request);

	return 0;
}

/**
 * PE_SRC_VDM_Identity_Request
 */
static int pe_src_vdm_identity_request(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_vdm_identity_request_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_vdm_identity_request_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_VDM_IDENTITY_REQUEST\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_VDM_IDENTITY_REQUEST;

	return 0;
}

static int pe_src_vdm_identity_request_run(int port)
{
	/*
	 * Discover identity of the Cable Plug
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_DISCOVER_VDM_IDENTITY_DONE) &&
			tc_is_vconn_src(port) &&
			pe[port].cable_discover_identity_count <=
						N_DISCOVER_IDENTITY_COUNT) {
		pe[port].cable_discover_identity_count++;

		pe[port].partner_is_cable = CABLE;
		pe[port].vdm_cmd = DISCOVER_IDENTITY;
		pe[port].vdm_data[0] = VDO(USB_SID_PD, 1, /* structured */
			VDO_SVDM_VERS(1) | DISCOVER_IDENTITY);
		pe[port].vdm_cnt = 1;

		return sm_set_state(port, PE_OBJ(port), pe_vdm_request);
	}

	return sm_set_state(port, PE_OBJ(port), pe_src_send_capabilities);
}

/**
 * PE_SRC_Discovery
 */
static int pe_src_discovery(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_discovery_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_discovery_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_DISCOVERY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_DISCOVERY;

	/*
	 * Initialize and run the SourceCapabilityTimer in order
	 * to trigger sending a Source_Capabilities Message.
	 *
	 * The SourceCapabilityTimer Shall continue to run during cable
	 * identity discover and Shall Not be initialized on re-entry
	 * to PE_SRC_Discovery.
	 */
	if (pe[port].obj.last_state != pe_vdm_request)
		pe[port].source_cap_timer =
				get_time().val + PD_T_SEND_SOURCE_CAP;

	return 0;
}

static int pe_src_discovery_run(int port)
{
	/*
	 * A VCONN or Charge-Through VCONN Powered Device was detected.
	 */
	if (pe[port].vpd_vdo >= 0 && VPD_VDO_CTS(pe[port].vpd_vdo))
		return sm_set_state(port, PE_OBJ(port), pe_src_disabled);

	/*
	 * Transition to the PE_SRC_Send_Capabilities state when:
	 *   1) The SourceCapabilityTimer times out and
	 *      CapsCounter ≤ nCapsCount.
	 *
	 * Transition to the PE_SRC_Disabled state when:
	 *   1) The Port Partners are not presently PD Connected
	 *   2) And the SourceCapabilityTimer times out
	 *   3) And CapsCounter > nCapsCount.
	 */
	if (get_time().val > pe[port].source_cap_timer) {
		if (pe[port].caps_counter <= N_CAPS_COUNT)
			sm_set_state(port, PE_OBJ(port),
						pe_src_send_capabilities);
		else if (!PE_CHK_FLAG(port, PE_FLAGS_PD_CONNECTION)) {
			sm_set_state(port, PE_OBJ(port), pe_src_disabled);
		}
		return 0;
	}

	/*
	 * Transition to the PE_SRC_Disabled state when:
	 *   1) The Port Partners have not been PD Connected.
	 *   2) And the NoResponseTimer times out.
	 *   3) And the HardResetCounter > nHardResetCount.
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_PD_CONNECTION) &&
			pe[port].no_response_timer > 0 &&
			get_time().val > pe[port].no_response_timer &&
			pe[port].hard_reset_counter > N_HARD_RESET_COUNT)
		return sm_set_state(port, PE_OBJ(port), pe_src_disabled);

	/*
	 * Discover identity of the Cable Plug
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_DISCOVER_VDM_IDENTITY_DONE) &&
	pe[port].cable_discover_identity_count < N_DISCOVER_IDENTITY_COUNT) {
		sm_set_state(port, PE_OBJ(port), pe_src_vdm_identity_request);
	}

	return 0;
}

/**
 * PE_SRC_Send_Capabilities
 */
static int pe_src_send_capabilities(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_send_capabilities_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_send_capabilities_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_SEND_CAPABILITIES\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_SEND_CAPABILITIES;

	/* Send PD Capabilities message */
	send_source_cap(port);

	/* Increment CapsCounter */
	pe[port].caps_counter++;

	/* Stop sender response timer */
	pe[port].sender_response_timer = 0;

	/*
	 * Clear PE_FLAGS_INTERRUPTIBLE_AMS flag if it was set
	 * in the src_discovery state
	 */
	PE_CLR_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);

	return 0;
}

static int pe_src_send_capabilities_run(int port)
{
	/*
	 * If a GoodCRC Message is received then the Policy Engine Shall:
	 *  1) Stop the NoResponseTimer.
	 *  2) Reset the HardResetCounter and CapsCounter to zero.
	 *  3) Initialize and run the SenderResponseTimer.
	 */
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL) &&
				pe[port].sender_response_timer == 0) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/* Stop the NoResponseTimer */
		pe[port].no_response_timer = 0;

		/* Reset the HardResetCounter to zero */
		pe[port].hard_reset_counter = 0;

		/* Reset the CapsCounter to zero */
		pe[port].caps_counter = 0;

		/* Initialize and run the SenderResponseTimer */
		pe[port].sender_response_timer = get_time().val +
							PD_T_SENDER_RESPONSE;
	}

	/*
	 * Transition to the PE_SRC_Negotiate_Capability state when:
	 *  1) A Request Message is received from the Sink
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		/*
		 * Request Message Received?
		 */
		if (PD_HEADER_CNT(emsg[port].header) > 0 &&
			PD_HEADER_TYPE(emsg[port].header) == PD_DATA_REQUEST) {

			/*
			 * Set to highest revision supported by both
			 * ports.
			 */
			prl_set_rev(port,
				(PD_HEADER_REV(emsg[port].header) > PD_REV30) ?
				PD_REV30 : PD_HEADER_REV(emsg[port].header));

			/* We are PD connected */
			PE_SET_FLAG(port, PE_FLAGS_PD_CONNECTION);
			tc_pd_connection(port, 1);

			/*
			 * Handle the Sink Request in
			 * PE_SRC_Negotiate_Capability state
			 */
			return sm_set_state(port, PE_OBJ(port),
					pe_src_negotiate_capability);
		}

		/* We have a Protocol Error. Send Soft Reset Message */
		return sm_set_state(port, PE_OBJ(port), pe_send_soft_reset);
	}

	/*
	 * Transition to the PE_SRC_Discovery state when:
	 *  1) The Protocol Layer indicates that the Message has not been sent
	 *     and we are presently not Connected
	 *
	 * NOTE: The PE_FLAGS_PROTOCOL_ERROR is set if a GoodCRC Message
	 *       is not received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR) &&
			!PE_CHK_FLAG(port, PE_FLAGS_PD_CONNECTION)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);

		return sm_set_state(port, PE_OBJ(port), pe_src_discovery);
	}

	/*
	 * Transition to the PE_SRC_Disabled state when:
	 *  1) The Port Partners have not been PD Connected
	 *  2) The NoResponseTimer times out
	 *  3) And the HardResetCounter > nHardResetCount.
	 *
	 * Transition to the Error Recovery state when:
	 *  1) The Port Partners have previously been PD Connected
	 *  2) The NoResponseTimer times out
	 *  3) And the HardResetCounter > nHardResetCount.
	 */
	if (pe[port].no_response_timer > 0 &&
			get_time().val > pe[port].no_response_timer &&
			pe[port].hard_reset_counter > N_HARD_RESET_COUNT) {
		if (PE_CHK_FLAG(port, PE_FLAGS_PD_CONNECTION))
			sm_set_state(port, PE_OBJ(port),
						pe_wait_for_error_recovery);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_disabled);
		return 0;
	}

	/*
	 * Transition to the PE_SRC_Hard_Reset state when:
	 *  1) The SenderResponseTimer times out.
	 */
	if ((pe[port].sender_response_timer > 0) &&
			get_time().val > pe[port].sender_response_timer) {
		sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
	}

	return 0;
}

/**
 * PE_SRC_Negotiate_Capability
 */
static int pe_src_negotiate_capability(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_negotiate_capability_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_negotiate_capability_entry(int port)
{
	/* Get message payload */
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);

	DEBUG_PRINTF("C%d: PE_SRC_NEGOTIATE_CAPABILITY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_NEGOTIATE_CAPABILITY;

	/*
	 * Evaluate the Request from the Attached Sink
	 */

	/*
	 * Transition to the PE_SRC_Capability_Response state when:
	 *  1) The Request cannot be met.
	 *  2) Or the Request can be met later from the Power Reserve
	 *
	 * Transition to the PE_SRC_Transition_Supply state when:
	 *  1) The Request can be met
	 *
	 */
	if (pd_check_requested_voltage(payload, port) != EC_SUCCESS) {
		sm_set_state(port, PE_OBJ(port), pe_src_capability_response);
	} else {
		PE_SET_FLAG(port, PE_FLAGS_ACCEPT);
		pe[port].requested_idx = RDO_POS(payload);
		sm_set_state(port, PE_OBJ(port), pe_src_transition_supply);
	}

	return 0;
}

/**
 * PE_SRC_Transition_Supply
 */
static int pe_src_transition_supply(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_transition_supply_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_transition_supply_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_TRANSITION_SUPPLY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_TRANSITION_SUPPLY;

	/* Transition Power Supply */
	pd_transition_voltage(pe[port].requested_idx);

	/* Send a GotoMin Message or otherwise an Accept Message */
	if (PE_CHK_FLAG(port, PE_FLAGS_ACCEPT)) {
		PE_CLR_FLAG(port, PE_FLAGS_ACCEPT);

		pe[port].timeout = get_time().val + PD_T_SINK_TRANSITION +
						PD_POWER_SUPPLY_TURN_ON_DELAY;
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	} else {
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_GOTO_MIN);
	}

	return 0;
}

static int pe_src_transition_supply_run(int port)
{
	if (get_time().val < pe[port].timeout)
		return 0;

	/*
	 * Transition to the PE_SRC_Ready state when:
	 *  1) The power supply is ready.
	 *
	 *  NOTE: This code block is executed twice:
	 *        First Pass)
	 *            When PE_FLAGS_TX_COMPLETE is set due to the
	 *            PD_CTRL_ACCEPT or PD_CTRL_GOTO_MIN messages
	 *            being sent.
	 *
	 *        Second Pass)
	 *            When PE_FLAGS_TX_COMPLETE is set due to the
	 *            PD_CTRL_PS_RDY message being sent.
	 */
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/*
		 * NOTE: If a message was received,
		 * pe_src_ready state will handle it.
		 */

		if (PE_CHK_FLAG(port, PE_FLAGS_PS_READY)) {
			PE_CLR_FLAG(port, PE_FLAGS_PS_READY);
			/* NOTE: Second pass through this code block */
			/* Explicit Contract is now in place */
			PE_SET_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		} else {
			/* NOTE: First pass through this code block */
			/* Send PS_RDY message */
			prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
			PE_SET_FLAG(port, PE_FLAGS_PS_READY);
		}

		return 0;
	}

	/*
	 * Transition to the PE_SRC_Hard_Reset state when:
	 *  1) A Protocol Error occurs.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);
		sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
	}

	return 0;
}

/**
 * PE_SRC_Ready
 */
static int pe_src_ready(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_ready_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_ready_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_READY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_READY;

	/*
	 * If the transition into PE_SRC_Ready is the result of Protocol Error
	 * that has not caused a Soft Reset (see Section 8.3.3.4.1) then the
	 * notification to the Protocol Layer of the end of the AMS Shall Not
	 * be sent since there is a Message to be processed.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);
	} else {
		PE_CLR_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);
		prl_end_ams(port);
	}

	/*
	 * Do port partner discovery
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION |
				PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE) &&
				pe[port].port_discover_identity_count <=
						N_DISCOVER_IDENTITY_COUNT) {
		pe[port].discover_identity_timer =
				get_time().val + PD_T_DISCOVER_IDENTITY;
	} else {
		PE_SET_FLAG(port, PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE);
		pe[port].discover_identity_timer = 0;
	}

	/* NOTE: PPS Implementation should be added here. */

	tc_set_timeout(port, 5 * MSEC);

	return 0;
}

static int pe_src_ready_run(int port)
{
	uint32_t payload;
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	/*
	 * Transitions to the PE_INIT_PORT_VDM_Identity_Request state when:
	 *   1) The DiscoverIdentityTimer times out.
	 */
	if (pe[port].discover_identity_timer > 0 &&
			get_time().val > pe[port].discover_identity_timer) {
		pe[port].port_discover_identity_count++;
		pe[port].vdm_cmd = DO_PORT_DISCOVERY_START;
		PE_CLR_FLAG(port, PE_FLAGS_VDM_REQUEST_NAKED |
						PE_FLAGS_VDM_REQUEST_BUSY);
		return sm_set_state(port, PE_OBJ(port), pe_do_port_discovery);
	}

	/*
	 * Handle Device Policy Manager Requests
	 */

	/*
	 * Ignore sink specific request:
	 *   DPM_REQUEST_NEW_POWER_LEVEL
	 *   DPM_REQUEST_SOURCE_CAP
	 */

	PE_CLR_DPM_REQUEST(port, DPM_REQUEST_NEW_POWER_LEVEL |
				DPM_REQUEST_SOURCE_CAP);

	if (pe[port].dpm_request) {
		if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_DR_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_DR_SWAP);
			if (PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION))
				sm_set_state(port, PE_OBJ(port),
							pe_src_hard_reset);
			else
				sm_set_state(port, PE_OBJ(port),
							pe_drs_send_swap);
		} else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_PR_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_PR_SWAP);
			sm_set_state(port, PE_OBJ(port),
						pe_prs_src_snk_send_swap);
		}
#ifdef CONFIG_USBC_VCONN
		else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_VCONN_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_VCONN_SWAP);
			sm_set_state(port, PE_OBJ(port), pe_vcs_send_swap);
		}
#endif
		else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_GOTO_MIN)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_GOTO_MIN);
			sm_set_state(port, PE_OBJ(port),
						pe_src_transition_supply);
		} else if (PE_CHK_DPM_REQUEST(port,
						DPM_REQUEST_SRC_CAP_CHANGE)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_SRC_CAP_CHANGE);
			sm_set_state(port, PE_OBJ(port),
						pe_src_send_capabilities);
		} else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_SEND_PING)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_SEND_PING);
			sm_set_state(port, PE_OBJ(port), pe_src_ping);
		} else if (PE_CHK_DPM_REQUEST(port,
					DPM_REQUEST_DISCOVER_IDENTITY)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_DISCOVER_IDENTITY);

			pe[port].partner_is_cable = CABLE;
			pe[port].vdm_cmd = DISCOVER_IDENTITY;
			pe[port].vdm_data[0] = VDO(
					USB_SID_PD,
					1, /* structured */
					VDO_SVDM_VERS(1) | DISCOVER_IDENTITY);
			pe[port].vdm_cnt = 1;
			sm_set_state(port, PE_OBJ(port), pe_vdm_request);
		}
		return 0;
	}

	/*
	 * Handle Source Requests
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);
		payload = *(uint32_t *)emsg[port].buf;

		/* Extended Message Requests */
		if (ext > 0) {
			switch (type) {
			case PD_EXT_GET_BATTERY_CAP:
				sm_set_state(port, PE_OBJ(port),
						pe_give_battery_cap);
				break;
			case PD_EXT_GET_BATTERY_STATUS:
				sm_set_state(port, PE_OBJ(port),
						pe_give_battery_status);
				break;
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
		/* Data Message Requests */
		else if (cnt > 0) {
			switch (type) {
			case PD_DATA_REQUEST:
				sm_set_state(port, PE_OBJ(port),
					pe_src_negotiate_capability);
				break;
			case PD_DATA_SINK_CAP:
				break;
			case PD_DATA_VENDOR_DEF:
				if (PD_HEADER_TYPE(emsg[port].header) ==
							PD_DATA_VENDOR_DEF) {
					if (PD_VDO_SVDM(payload)) {
						sm_set_state(port,
						PE_OBJ(port), pe_vdm_response);
					} else
						sm_set_state(port, PE_OBJ(port),
						pe_handle_custom_vdm_request);
				}
				break;
			case PD_DATA_BIST:
				sm_set_state(port, PE_OBJ(port), pe_bist);
				break;
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
		/* Control Message Requests */
		else {
			switch (type) {
			case PD_CTRL_GOOD_CRC:
				break;
			case PD_CTRL_NOT_SUPPORTED:
				break;
			case PD_CTRL_PING:
				break;
			case PD_CTRL_GET_SOURCE_CAP:
				sm_set_state(port, PE_OBJ(port),
					pe_src_send_capabilities);
				break;
			case PD_CTRL_GET_SINK_CAP:
				sm_set_state(port, PE_OBJ(port),
						pe_snk_give_sink_cap);
				break;
			case PD_CTRL_GOTO_MIN:
				break;
			case PD_CTRL_PR_SWAP:
				sm_set_state(port, PE_OBJ(port),
						pe_prs_src_snk_evaluate_swap);
				break;
			case PD_CTRL_DR_SWAP:
				if (PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION))
					return sm_set_state(port, PE_OBJ(port),
							pe_src_hard_reset);

				sm_set_state(port, PE_OBJ(port),
							pe_drs_evaluate_swap);
				break;
#ifdef CONFIG_USBC_VCONN
			case PD_CTRL_VCONN_SWAP:
				sm_set_state(port, PE_OBJ(port),
							pe_vcs_evaluate_swap);
				break;
#endif
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
	}
	return 0;
}

static int pe_src_ready_exit(int port)
{
	PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

	/*
	 * If the Source is initiating an AMS then the Policy Engine Shall
	 * notify the Protocol Layer that the first Message in an AMS will
	 * follow.
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS))
		prl_start_ams(port);

	tc_set_timeout(port, 2 * MSEC);

	return 0;
}

/**
 * PE_SRC_Disabled
 */
static int pe_src_disabled(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_disabled_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_disabled_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_DISABLED\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_DISABLED;

	if ((pe[port].vpd_vdo >= 0) && VPD_VDO_CTS(pe[port].vpd_vdo)) {
		/*
		 * Inform the Device Policy Manager that a Charge-Through VCONN
		 * Powered Device was detected.
		 */
		tc_ctvpd_detected(port);
	}

	/*
	 * Unresponsive to USB Power Delivery messaging, but not to Hard Reset
	 * Signaling. See pe_got_hard_reset
	 */

	return 0;
}

/**
 * PE_SRC_Capability_Response
 */
static int pe_src_capability_response(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_capability_response_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_capability_response_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_CAPABILITY_RESPONSE\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_CAPABILITY_RESPONSE;

	/* NOTE: Wait messaging should be implemented. */

	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);

	return 0;
}

static int pe_src_capability_response_run(int port)
{
	/*
	 * Transition to the PE_SRC_Ready state when:
	 *  1) There is an Explicit Contract and
	 *  2) A Reject Message has been sent and the present Contract is still
	 *     Valid or
	 *  3) A Wait Message has been sent.
	 *
	 * Transition to the PE_SRC_Hard_Reset state when:
	 *  1) There is an Explicit Contract and
	 *  2) The Reject Message has been sent and the present
	 *     Contract is Invalid
	 *
	 * Transition to the PE_SRC_Wait_New_Capabilities state when:
	 *  1) There is no Explicit Contract and
	 *  2) A Reject Message has been sent or
	 *  3) A Wait Message has been sent.
	 */
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (PE_CHK_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT))
			/*
			 * NOTE: The src capabilities listed in
			 *       board/xxx/usb_pd_policy.c will not
			 *       change so the present contract will
			 *       never be invalid.
			 */
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			/*
			 * NOTE: The src capabilities listed in
			 *       board/xxx/usb_pd_policy.c will not
			 *       change, so no need to resending them
			 *       again. Transition to disabled state.
			 */
			sm_set_state(port, PE_OBJ(port), pe_src_disabled);
	}

	return 0;
}

/**
 * PE_SRC_Hard_Reset
 */
static int pe_src_hard_reset(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_hard_reset_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_hard_reset_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_HARD_RESET\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_HARD_RESET;

	/* Generate Hard Reset Signal */
	prl_execute_hard_reset(port);

	/* Increment the HardResetCounter */
	pe[port].hard_reset_counter++;

	/* Start NoResponseTimer */
	pe[port].no_response_timer = get_time().val + PD_T_NO_RESPONSE;

	/* Start PSHardResetTimer */
	pe[port].ps_hard_reset_timer = get_time().val + PD_T_PS_HARD_RESET;

	return 0;
}

static int pe_src_hard_reset_run(int port)
{
	/*
	 * Transition to the PE_SRC_Transition_to_default state when:
	 *  1) The PSHardResetTimer times out.
	 */
	if (get_time().val > pe[port].ps_hard_reset_timer)
		sm_set_state(port, PE_OBJ(port), pe_src_transition_to_default);

	return 0;
}

/**
 * PE_SRC_Hard_Reset_Received
 */
static int pe_src_hard_reset_received(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_hard_reset_received_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_hard_reset_received_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_HARD_RESET_RECEIVED\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_HARD_RESET_RECEIVED;

	/* Start NoResponseTimer */
	pe[port].no_response_timer = get_time().val + PD_T_NO_RESPONSE;

	/* Start PSHardResetTimer */
	pe[port].ps_hard_reset_timer = get_time().val + PD_T_PS_HARD_RESET;

	return 0;
}

static int pe_src_hard_reset_received_run(int port)
{
	/*
	 * Transition to the PE_SRC_Transition_to_default state when:
	 *  1) The PSHardResetTimer times out.
	 */
	if (get_time().val > pe[port].ps_hard_reset_timer)
		sm_set_state(port, PE_OBJ(port), pe_src_transition_to_default);

	return 0;
}

/**
 * PE_SRC_Transition_To_Default
 */
static int pe_src_transition_to_default(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_src_transition_to_default_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_transition_to_default_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_TRANSITION_TO_DEFAULT\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_TRANSITION_TO_DEFAULT;

	/* Reset flags */
	pe[port].flags = 0;

	/* Reset DPM Request */
	pe[port].dpm_request = 0;

	/*
	 * Request Device Policy Manager to request power
	 * supply Hard Resets to vSafe5V via vSafe0V
	 * Reset local HW
	 * Request Device Policy Manager to set Port Data
	 * Role to DFP and turn off VCONN
	 */
	tc_hard_reset(port);

	return 0;
}

static int pe_src_transition_to_default_run(int port)
{
	/*
	 * Transition to the PE_SRC_Startup state when:
	 *   1) The power supply has reached the default level.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PS_RESET_COMPLETE)) {
		PE_CLR_FLAG(port, PE_FLAGS_PS_RESET_COMPLETE);
		/* Inform the Protocol Layer that the Hard Reset is complete */
		prl_hard_reset_complete(port);
		sm_set_state(port, PE_OBJ(port), pe_src_startup);
	}

	return 0;
}

/**
 * PE_SNK_Startup State
 */
static int pe_snk_startup(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_startup_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_startup_entry(int port)
{
	DEBUG_PRINTF("C%d: SNK_START\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_STARTUP;

	/* Reset the protocol layer */
	prl_reset(port);

	/* Set initial data role */
	pe[port].data_role = tc_get_data_role(port);

	/* Set initial power role */
	pe[port].power_role = PD_ROLE_SINK;

	/* Clear explicit contract */
	PE_CLR_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);

	return 0;
}

static int pe_snk_startup_run(int port)
{
	/* Wait until protocol layer is done resetting */
	if (PE_CHK_FLAG(port, PE_FLAGS_PRL_RESET_PENDING))
		return 0;

	/*
	 * Once the reset process completes, the Policy Engine Shall
	 * transition to the PE_SNK_Discovery state
	 */
	sm_set_state(port, PE_OBJ(port), pe_snk_discovery);

	return 0;
}

/**
 * PE_SNK_Discovery State
 */
static int pe_snk_discovery(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_discovery_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_discovery_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_DISCOVERY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_DISCOVERY;
	return 0;
}

static int pe_snk_discovery_run(int port)
{
	/*
	 * Transition to the PE_SNK_Wait_for_Capabilities state when:
	 *   1) VBUS has been detected
	 */
	if (pd_is_vbus_present(port))
		sm_set_state(port, PE_OBJ(port), pe_snk_wait_for_capabilities);

	return 0;
}

/**
 * PE_SNK_Wait_For_Capabilities State
 */
static int pe_snk_wait_for_capabilities(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_wait_for_capabilities_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_wait_for_capabilities_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_WAIT_FOR_CAPABILITIES\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_WAIT_FOR_CAPABILITIES;

	/* Initialize and start the SinkWaitCapTimer */
	pe[port].timeout = get_time().val + PD_T_SINK_WAIT_CAP;

	return 0;
}

static int pe_snk_wait_for_capabilities_run(int port)
{
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	/*
	 * Transition to the PE_SNK_Evaluate_Capability state when:
	 *  1) A Source_Capabilities Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt > 0) && (type == PD_DATA_SOURCE_CAP)) {
			sm_set_state(port, PE_OBJ(port),
						pe_snk_evaluate_capability);
			return 0;
		}
	}

	/* When the SinkWaitCapTimer times out, perform a Hard Reset. */
	if (get_time().val > pe[port].timeout) {
		PE_SET_FLAG(port, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT);
		sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
	}

	return 0;
}

/**
 * PE_SNK_Evaluate_Capability State
 */
static int pe_snk_evaluate_capability(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_evaluate_capability_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_evaluate_capability_entry(int port)
{
	uint32_t *pdo = (uint32_t *)emsg[port].buf;
	uint32_t header = emsg[port].header;
	uint32_t num = emsg[port].len >> 2;
	int i;

	DEBUG_PRINTF("C%d: PE_SNK_EVALUATE_CAPABILITY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_EVALUATE_CAPABILITY;

	/* Reset Hard Reset counter to zero */
	pe[port].hard_reset_counter = 0;

	/* Set to highest revision supported by both ports. */
	prl_set_rev(port, (PD_HEADER_REV(header) > PD_REV30) ?
					PD_REV30 : PD_HEADER_REV(header));

	pe[port].src_cap_cnt = num;

	for (i = 0; i < num; i++)
		pe[port].src_caps[i] = *pdo++;

	/* src cap 0 should be fixed PDO */
	pe_update_pdo_flags(port, pdo[0]);

	/* Evaluate the options based on supplied capabilities */
	pd_process_source_cap(port, pe[port].src_cap_cnt, pe[port].src_caps);

	/* We are PD Connected */
	PE_SET_FLAG(port, PE_FLAGS_PD_CONNECTION);
	tc_pd_connection(port, 1);

	/* Device Policy Response Received */
	sm_set_state(port, PE_OBJ(port), pe_snk_select_capability);

	return 0;
}

/**
 * PE_SNK_Select_Capability State
 */
static int pe_snk_select_capability(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_select_capability_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_select_capability_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_SELECT_CAPABILITY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_SELECT_CAPABILITY;

	pe[port].sender_response_timer = 0;
	/* Send Request */
	pe_send_request_msg(port);

	return 0;
}

static int pe_snk_select_capability_run(int port)
{
	uint8_t type;
	uint8_t cnt;

	/* Wait until message is sent */
	if (pe[port].sender_response_timer == 0 &&
			(PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL))) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/* Initialize and run SenderResponseTimer */
		pe[port].sender_response_timer =
					get_time().val + PD_T_SENDER_RESPONSE;
	}

	if (pe[port].sender_response_timer == 0)
		return 0;

	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);
		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);

		/*
		 * Transition to the PE_SNK_Transition_Sink state when:
		 *  1) An Accept Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Wait_for_Capabilities state when:
		 *  1) There is no Explicit Contract in place and
		 *  2) A Reject Message is received from the Source or
		 *  3) A Wait Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Ready state when:
		 *  1) There is an Explicit Contract in place and
		 *  2) A Reject Message is received from the Source or
		 *  3) A Wait Message is received from the Source.
		 *
		 * Transition to the PE_SNK_Hard_Reset state when:
		 *  1) A SenderResponseTimer timeout occurs.
		 */

		/* Only look at control messages */
		if (cnt == 0) {
			/*
			 * Accept Message Received
			 */
			if (type == PD_CTRL_ACCEPT) {
				/* explicit contract is now in place */
				PE_SET_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);
				return sm_set_state(port, PE_OBJ(port),
						pe_snk_transition_sink);
			}
			/*
			 * Reject or Wait Message Received
			 */
			else if (type == PD_CTRL_REJECT ||
							type == PD_CTRL_WAIT) {
				if (type == PD_CTRL_WAIT)
					PE_SET_FLAG(port, PE_FLAGS_WAIT);

				/*
				 * We had a previous explicit contract, so
				 * transition to PE_SNK_Ready
				 */
				if (PE_CHK_FLAG(port,
						PE_FLAGS_EXPLICIT_CONTRACT))
					sm_set_state(port,
						PE_OBJ(port), pe_snk_ready);
				/*
				 * No previous explicit contract, so transition
				 * to PE_SNK_Wait_For_Capabilities
				 */
				else
					sm_set_state(port, PE_OBJ(port),
						pe_snk_wait_for_capabilities);
				return 0;
			}
		}
	}

	/* SenderResponsetimer timeout */
	if (get_time().val > pe[port].sender_response_timer)
		sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);

	return 0;
}

/**
 * PE_SNK_Transition_Sink State
 */
static int pe_snk_transition_sink(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_transition_sink_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_transition_sink_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_TRANSITION_SINK\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_TRANSITION_SINK;

	/* Initialize and run PSTransitionTimer */
	pe[port].ps_transition_timer = get_time().val + PD_T_PS_TRANSITION;
	return 0;
}

static int pe_snk_transition_sink_run(int port)
{
	/*
	 * Transition to the PE_SNK_Ready state when:
	 *  1) A PS_RDY Message is received from the Source.
	 *
	 * Transition to the PE_SNK_Hard_Reset state when:
	 *  1) A Protocol Error occurs.
	 */

	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		/*
		 * PS_RDY message received
		 */
		if ((PD_HEADER_CNT(emsg[port].header) == 0) &&
			   (PD_HEADER_TYPE(emsg[port].header) ==
			   PD_CTRL_PS_RDY))
			return sm_set_state(port, PE_OBJ(port), pe_snk_ready);

		/*
		 * Protocol Error
		 */
		sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
	}

	/*
	 * Timeout will lead to a Hard Reset
	 */
	if (get_time().val > pe[port].ps_transition_timer &&
			pe[port].hard_reset_counter <= N_HARD_RESET_COUNT) {
		PE_SET_FLAG(port, PE_FLAGS_PS_TRANSITION_TIMEOUT);

		sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
	}

	return 0;
}

static int pe_snk_transition_sink_exit(int port)
{
	/* Transition Sink's power supply to the new power level */
	pd_set_input_current_limit(port,
				pe[port].curr_limit, pe[port].supply_voltage);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		/* Set ceiling based on what's negotiated */
		charge_manager_set_ceil(port,
				CEIL_REQUESTOR_PD, pe[port].curr_limit);
	return 0;
}

/**
 * PE_SNK_Ready State
 */
static int pe_snk_ready(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_ready_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_ready_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_READY\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_READY;

	PE_CLR_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);
	prl_end_ams(port);

	/*
	 * On entry to the PE_SNK_Ready state as the result of a wait, then do
	 * the following:
	 *   1) Initialize and run the SinkRequestTimer
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_WAIT)) {
		PE_CLR_FLAG(port, PE_FLAGS_WAIT);
		pe[port].sink_request_timer =
					get_time().val + PD_T_SINK_REQUEST;
	} else {
		pe[port].sink_request_timer = 0;
	}

	/*
	 * Do port partner discovery
	 */
	if (!PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION |
				PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE) &&
				pe[port].port_discover_identity_count <=
						N_DISCOVER_IDENTITY_COUNT) {
		pe[port].discover_identity_timer =
			get_time().val + PD_T_DISCOVER_IDENTITY;
	} else {
		PE_SET_FLAG(port, PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE);
		pe[port].discover_identity_timer = 0;
	}

	/*
	 * On entry to the PE_SNK_Ready state if the current Explicit Contract
	 * is for a PPS APDO, then do the following:
	 *  1) Initialize and run the SinkPPSPeriodicTimer.
	 *  NOTE: PPS Implementation should be added here.
	 */

	tc_set_timeout(port, 5 * MSEC);

	return 0;
}

static int pe_snk_ready_run(int port)
{
	uint32_t payload;
	uint8_t type;
	uint8_t cnt;
	uint8_t ext;

	if (pe[port].sink_request_timer > 0 &&
				get_time().val > pe[port].sink_request_timer) {
		sm_set_state(port, PE_OBJ(port), pe_snk_select_capability);
		return 0;
	}

	/*
	 * Transitions to the PE_INIT_PORT_VDM_Identity_Request state when:
	 *   1) The DiscoverIdentityTimer times out.
	 */
	if (pe[port].discover_identity_timer > 0 &&
			get_time().val > pe[port].discover_identity_timer) {
		pe[port].port_discover_identity_count++;
		pe[port].vdm_cmd = DO_PORT_DISCOVERY_START;
		PE_CLR_FLAG(port, PE_FLAGS_VDM_REQUEST_NAKED |
						PE_FLAGS_VDM_REQUEST_BUSY);
		return sm_set_state(port, PE_OBJ(port), pe_do_port_discovery);
	}

	/*
	 * Handle Device Policy Manager Requests
	 */
	/*
	 * Ignore source specific requests:
	 *   DPM_REQUEST_GOTO_MIN
	 *   DPM_REQUEST_SRC_CAP_CHANGE,
	 *   DPM_REQUEST_GET_SNK_CAPS,
	 *   DPM_REQUEST_SEND_PING
	 */
	PE_CLR_DPM_REQUEST(port, DPM_REQUEST_GOTO_MIN |
				DPM_REQUEST_SRC_CAP_CHANGE |
				DPM_REQUEST_GET_SNK_CAPS |
				DPM_REQUEST_SEND_PING);

	if (pe[port].dpm_request) {
		if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_DR_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_DR_SWAP);
			if (PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION))
				sm_set_state(port, PE_OBJ(port),
							pe_snk_hard_reset);
			else
				sm_set_state(port, PE_OBJ(port),
							pe_drs_send_swap);
		} else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_PR_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_PR_SWAP);
			sm_set_state(port, PE_OBJ(port),
						pe_prs_snk_src_send_swap);
		}
#ifdef CONFIG_USBC_VCONN
		else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_VCONN_SWAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_VCONN_SWAP);
			sm_set_state(port, PE_OBJ(port), pe_vcs_send_swap);
		}
#endif
		else if (PE_CHK_DPM_REQUEST(port, DPM_REQUEST_SOURCE_CAP)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_SOURCE_CAP);
			sm_set_state(port, PE_OBJ(port), pe_snk_get_source_cap);
		} else if (PE_CHK_DPM_REQUEST(port,
					DPM_REQUEST_NEW_POWER_LEVEL)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_NEW_POWER_LEVEL);
			sm_set_state(port, PE_OBJ(port),
						pe_snk_select_capability);
		} else if (PE_CHK_DPM_REQUEST(port,
					DPM_REQUEST_DISCOVER_IDENTITY)) {
			PE_CLR_DPM_REQUEST(port, DPM_REQUEST_DISCOVER_IDENTITY);

			pe[port].partner_is_cable = CABLE;
			pe[port].vdm_cmd = DISCOVER_IDENTITY;
			pe[port].vdm_data[0] = VDO(
				USB_SID_PD,
				1, /* structured */
				VDO_SVDM_VERS(1) | DISCOVER_IDENTITY);
			pe[port].vdm_cnt = 1;

			sm_set_state(port, PE_OBJ(port), pe_vdm_request);
		}
		return 0;
	}

	/*
	 * Handle Source Requests
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);
		payload = *(uint32_t *)emsg[port].buf;

		/* Extended Message Request */
		if (ext > 0) {
			switch (type) {
			case PD_EXT_GET_BATTERY_CAP:
				sm_set_state(port, PE_OBJ(port),
						pe_give_battery_cap);
				break;
			case PD_EXT_GET_BATTERY_STATUS:
				sm_set_state(port, PE_OBJ(port),
						pe_give_battery_status);
				break;
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
		/* Data Messages */
		else if (cnt > 0) {
			switch (type) {
			case PD_DATA_SOURCE_CAP:
				sm_set_state(port, PE_OBJ(port),
					pe_snk_evaluate_capability);
				break;
			case PD_DATA_VENDOR_DEF:
				if (PD_HEADER_TYPE(emsg[port].header) ==
							PD_DATA_VENDOR_DEF) {
					if (PD_VDO_SVDM(payload))
						sm_set_state(port, PE_OBJ(port),
							pe_vdm_response);
					else
						sm_set_state(port, PE_OBJ(port),
						pe_handle_custom_vdm_request);
				}
				break;
			case PD_DATA_BIST:
				sm_set_state(port, PE_OBJ(port), pe_bist);
				break;
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
		/* Control Messages */
		else {
			switch (type) {
			case PD_CTRL_GOOD_CRC:
				/* Do nothing */
				break;
			case PD_CTRL_PING:
				/* Do noghing */
				break;
			case PD_CTRL_GET_SOURCE_CAP:
				sm_set_state(port, PE_OBJ(port),
						pe_snk_give_source_cap);
				break;
			case PD_CTRL_GET_SINK_CAP:
				sm_set_state(port, PE_OBJ(port),
						pe_snk_give_sink_cap);
				break;
			case PD_CTRL_GOTO_MIN:
				sm_set_state(port, PE_OBJ(port),
					pe_snk_transition_sink);
				break;
			case PD_CTRL_PR_SWAP:
				sm_set_state(port, PE_OBJ(port),
						pe_prs_snk_src_evaluate_swap);
				break;
			case PD_CTRL_DR_SWAP:
				if (PE_CHK_FLAG(port, PE_FLAGS_MODAL_OPERATION))
					sm_set_state(port, PE_OBJ(port),
							pe_snk_hard_reset);
				else
					sm_set_state(port, PE_OBJ(port),
							pe_drs_evaluate_swap);
				break;
#ifdef CONFIG_USBC_VCONN
			case PD_CTRL_VCONN_SWAP:
				sm_set_state(port, PE_OBJ(port),
						pe_vcs_evaluate_swap);
				break;
#endif
			case PD_CTRL_NOT_SUPPORTED:
				/* Do nothing */
				break;
			default:
				sm_set_state(port, PE_OBJ(port),
					pe_send_not_supported);
			}
		}
	}

	return 0;
}

static int pe_snk_ready_exit(int port)
{
	if (!PE_CHK_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS))
		prl_start_ams(port);

	tc_set_timeout(port, 2 * MSEC);

	return 0;
}

/**
 * PE_SNK_Hard_Reset
 */
static int pe_snk_hard_reset(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_hard_reset_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_hard_reset_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_HARD_RESET\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_HARD_RESET;

	/*
	 * Note: If the SinkWaitCapTimer times out and the HardResetCounter is
	 *       greater than nHardResetCount the Sink Shall assume that the
	 *       Source is non-responsive.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT) &&
			pe[port].hard_reset_counter > N_HARD_RESET_COUNT) {
		sm_set_state(port, PE_OBJ(port), pe_src_disabled);
	}

	PE_CLR_FLAG(port, PE_FLAGS_SNK_WAIT_CAP_TIMEOUT);

	/* Request the generation of Hard Reset Signaling by the PHY Layer */
	pe_prl_execute_hard_reset(port);

	/* Increment the HardResetCounter */
	pe[port].hard_reset_counter++;

	/*
	 * Transition the Sink’s power supply to the new power level if
	 * PSTransistionTimer timeout occurred.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PS_TRANSITION_TIMEOUT)) {
		PE_SET_FLAG(port, PE_FLAGS_PS_TRANSITION_TIMEOUT);

		/* Transition Sink's power supply to the new power level */
		pd_set_input_current_limit(port, pe[port].curr_limit,
						pe[port].supply_voltage);
#ifdef CONFIG_CHARGE_MANAGER
		/* Set ceiling based on what's negotiated */
		charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							pe[port].curr_limit);
#endif
	}

	return 0;
}

static int pe_snk_hard_reset_run(int port)
{
	/*
	 * Transition to the PE_SNK_Transition_to_default state when:
	 *  1) The Hard Reset is complete.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_HARD_RESET_PENDING))
		return 0;

	sm_set_state(port, PE_OBJ(port), pe_snk_transition_to_default);
	return 0;
}

/**
 * PE_SNK_Transition_to_default
 */
static int pe_snk_transition_to_default(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_transition_to_default_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_transition_to_default_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_TRANSITION_TO_DEFAULT\n", port);

	pe[port].state_id = PE_SNK_TRANSITION_TO_DEFAULT;

	tc_hard_reset(port);

	return 0;
}

static int pe_snk_transition_to_default_run(int port)
{
	if (PE_CHK_FLAG(port, PE_FLAGS_PS_RESET_COMPLETE)) {
		/* PE_SNK_Startup clears all flags */

		/* Inform the Protocol Layer that the Hard Reset is complete */
		prl_hard_reset_complete(port);
		sm_set_state(port, PE_OBJ(port), pe_snk_startup);
	}

	return 0;
}

/**
 * PE_SNK_Get_Source_Cap
 */
static int pe_snk_get_source_cap(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_get_source_cap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_get_source_cap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_GET_SOURCE_CAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_GET_SOURCE_CAP;

	/* Send a Get_Source_Cap Message */
	emsg[port].len = 0;
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_GET_SOURCE_CAP);

	return 0;
}

static int pe_snk_get_source_cap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

static int pe_snk_give_source_cap(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_give_source_cap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);

}

static int pe_snk_give_source_cap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_GIVE_SOURCE_CAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_GIVE_SOURCE_CAP;

	send_source_cap(port);
	return 0;
}

static int pe_snk_give_source_cap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

/**
 * PE_SNK_Send_Soft_Reset and PE_SRC_Send_Soft_Reset
 */
static int pe_send_soft_reset(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_send_soft_reset_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_send_soft_reset_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SEND_SOFT_RESET\n", port);

	/* Set state id */
	pe[port].state_id = PE_SEND_SOFT_RESET;

	/* Reset Protocol Layer */
	prl_reset(port);

	pe[port].sender_response_timer = 0;

	return 0;
}

static int pe_send_soft_reset_run(int port)
{
	int type;
	int cnt;
	int ext;

	/* Wait until protocol layer is done resetting */
	if (PE_CHK_FLAG(port, PE_FLAGS_PRL_RESET_PENDING))
		return 0;

	if (pe[port].sender_response_timer == 0) {
		/* Send Soft Reset message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_SOFT_RESET);

		/* Initialize and run SenderResponseTimer */
		pe[port].sender_response_timer =
					get_time().val + PD_T_SENDER_RESPONSE;
	}

	/*
	 * Transition to PE_SNK_Hard_Reset or PE_SRC_Hard_Reset on Sender
	 * Response Timer Timeout or Protocol Layer or Protocol Error
	 */
	if (get_time().val > pe[port].sender_response_timer ||
			PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);

		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
		return 0;
	}

	/*
	 * Transition to the PE_SNK_Send_Capabilities or
	 * PE_SRC_Send_Capabilities state when:
	 *   1) An Accept Message has been received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0) && (type == PD_CTRL_ACCEPT)) {
			if (pe[port].power_role == PD_ROLE_SINK)
				sm_set_state(port, PE_OBJ(port),
						pe_snk_wait_for_capabilities);
			else
				sm_set_state(port, PE_OBJ(port),
						pe_src_send_capabilities);
			return 0;
		}
	}

	return 0;
}

static int pe_send_soft_reset_exit(int port)
{
	/* Clear TX Complete Flag */
	PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
	return 0;
}

/**
 * PE_SNK_Soft_Reset and PE_SNK_Soft_Reset
 */
static int pe_soft_reset(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_soft_reset_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_soft_reset_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SOFT_RESET\n", port);

	/* Set state id */
	pe[port].state_id = PE_SOFT_RESET;

	pe[port].sender_response_timer = 0;

	return 0;
}

static int pe_soft_reset_run(int port)
{
	if (pe[port].sender_response_timer == 0) {
		/* Send Accept message */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
		pe[port].sender_response_timer++;
	}

	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port),
						pe_snk_wait_for_capabilities);
		else
			sm_set_state(port, PE_OBJ(port),
						pe_src_send_capabilities);
	} else if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);

		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
	}

	return 0;
}

/**
 * PE_SRC_Not_Supported and PE_SNK_Not_Supported
 */
static int pe_send_not_supported(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_send_not_supported_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_send_not_supported_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SEND_NOT_SUPPORTED\n", port);

	/* Set state id */
	pe[port].state_id = PE_SEND_NOT_SUPPORTED;

	/* Request the Protocol Layer to send a Not_Supported Message. */
	if (prl_get_rev(port) > PD_REV20)
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_NOT_SUPPORTED);
	else
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);

	return 0;
}

static int pe_send_not_supported_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

/**
 * PE_SRC_Ping
 */
static int pe_src_ping(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_src_ping_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_src_ping_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SRC_PING\n", port);

	/* Set state id */
	pe[port].state_id = PE_SRC_PING;

	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PING);
	return 0;
}

static int pe_src_ping_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		sm_set_state(port, PE_OBJ(port), pe_src_ready);
	}

	return 0;
}

/**
 * PE_Give_Battery_Cap
 */
static int pe_give_battery_cap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_give_battery_cap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_give_battery_cap_entry(int port)
{
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);
	uint16_t *msg = (uint16_t *)emsg[port].buf;

	DEBUG_PRINTF("C%d: PE_GIVE_BATTERY_CAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_GIVE_BATTERY_CAP;

	/* msg[0] - extended header is set by Protocol Layer */

	/* Set VID */
	msg[1] = USB_VID_GOOGLE;

	/* Set PID */
	msg[2] = CONFIG_USB_PID;

	if (battery_is_present()) {
		/*
		 * We only have one fixed battery,
		 * so make sure batt cap ref is 0.
		 */
		if (BATT_CAP_REF(payload) != 0) {
			/* Invalid battery reference */
			msg[3] = 0;
			msg[4] = 0;
			msg[5] = 1;
		} else {
			uint32_t v;
			uint32_t c;

			/*
			 * The Battery Design Capacity field shall return the
			 * Battery’s design capacity in tenths of Wh. If the
			 * Battery is Hot Swappable and is not present, the
			 * Battery Design Capacity field shall be set to 0. If
			 * the Battery is unable to report its Design Capacity,
			 * it shall return 0xFFFF
			 */
			msg[3] = 0xffff;

			/*
			 * The Battery Last Full Charge Capacity field shall
			 * return the Battery’s last full charge capacity in
			 * tenths of Wh. If the Battery is Hot Swappable and
			 * is not present, the Battery Last Full Charge Capacity
			 * field shall be set to 0. If the Battery is unable to
			 * report its Design Capacity, the Battery Last Full
			 * Charge Capacity field shall be set to 0xFFFF.
			 */
			msg[4] = 0xffff;

			if (battery_design_voltage(&v) == 0) {
				if (battery_design_capacity(&c) == 0) {
					/*
					 * Wh = (c * v) / 1000000
					 * 10th of a Wh = Wh * 10
					 */
					msg[3] = DIV_ROUND_NEAREST((c * v),
								100000);
				}

				if (battery_full_charge_capacity(&c) == 0) {
					/*
					 * Wh = (c * v) / 1000000
					 * 10th of a Wh = Wh * 10
					 */
					msg[4] = DIV_ROUND_NEAREST((c * v),
								100000);
				}
			}
		}
	}

	/* Extended Battery Cap data is 9 bytes */
	emsg[port].len = 9;

	prl_send_ext_data_msg(port, TCPC_TX_SOP, PD_EXT_BATTERY_CAP);

	return 0;
}

static int pe_give_battery_cap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

/**
 * PE_Give_Battery_Status
 */
static int pe_give_battery_status(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_give_battery_status_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_give_battery_status_entry(int port)
{
	uint32_t payload = *(uint32_t *)(&emsg[port].buf);
	uint32_t *msg = (uint32_t *)emsg[port].buf;

	DEBUG_PRINTF("C%d: PE_GIVE_BATTERY_STATUS\n", port);

	/* Set state id */
	pe[port].state_id = PE_GIVE_BATTERY_STATUS;

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
				*msg |= BSDO_CAP(DIV_ROUND_NEAREST((c * v),
								100000));
			}

			/* Battery is present */
			*msg |= BSDO_PRESENT;

			/*
			 * For drivers that are not smart battery compliant,
			 * battery_status() returns EC_ERROR_UNIMPLEMENTED and
			 * the battery is assumed to be idle.
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

	/* Battery Status data is 4 bytes */
	emsg[port].len = 4;

	prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_BATTERY_STATUS);

	return 0;
}

static int pe_give_battery_status_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		sm_set_state(port, PE_OBJ(port), pe_src_ready);
	}

	return 0;
}

/**
 * PE_DRS_Evaluate_Swap
 */
static int pe_drs_evaluate_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_drs_evaluate_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_drs_evaluate_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_DRS_EVALUATE_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_DRS_EVALUATE_SWAP;

	/* Get evaluation of Data Role Swap request from DPM */
	if (pd_check_data_swap(port, pe[port].data_role)) {
		PE_SET_FLAG(port, PE_FLAGS_ACCEPT);
		/*
		 * PE_DRS_UFP_DFP_Evaluate_Swap and
		 * PE_DRS_DFP_UFP_Evaluate_Swap states embedded here.
		 */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	} else {
		/*
		 * PE_DRS_UFP_DFP_Reject_Swap and PE_DRS_DFP_UFP_Reject_Swap
		 * states embedded here.
		 */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	}

	return 0;
}

static int pe_drs_evaluate_swap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/* Accept Message sent. Transtion to PE_DRS_Change */
		if (PE_CHK_FLAG(port, PE_FLAGS_ACCEPT)) {
			PE_CLR_FLAG(port, PE_FLAGS_ACCEPT);
			sm_set_state(port, PE_OBJ(port), pe_drs_change);
		} else {
			/*
			 * Message sent. Transition back to PE_SRC_Ready or
			 * PE_SNK_Ready.
			 */
			if (pe[port].power_role == PD_ROLE_SOURCE)
				sm_set_state(port, PE_OBJ(port), pe_src_ready);
			else
				sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		}
	}

	return 0;
}

/**
 * PE_DRS_Change
 */
static int pe_drs_change(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_drs_change_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_drs_change_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_DRS_CHANGE\n", port);

	/* Set state id */
	pe[port].state_id = PE_DRS_CHANGE;

	/*
	 * PE_DRS_UFP_DFP_Change_to_DFP and PE_DRS_DFP_UFP_Change_to_UFP
	 * states embedded here.
	 */
	/* Request DPM to change port data role */
	pd_request_data_swap(port);
	return 0;
}

static int pe_drs_change_run(int port)
{
	/* Wait until the data role is changed */
	if (pe[port].data_role == tc_get_data_role(port))
		return 0;

	/* Update the data role */
	pe[port].data_role = tc_get_data_role(port);

	/*
	 * Port changed. Transition back to PE_SRC_Ready or
	 * PE_SNK_Ready.
	 */
	if (pe[port].power_role == PD_ROLE_SINK)
		sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	else
		sm_set_state(port, PE_OBJ(port), pe_src_ready);

	return 0;
}

/**
 * PE_DRS_Send_Swap
 */
static int pe_drs_send_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_drs_send_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_drs_send_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_DRS_SEND_SWAP\n", port);

	/* Set State id */
	pe[port].state_id = PE_DRS_SEND_SWAP;

	/*
	 * PE_DRS_UFP_DFP_Send_Swap and PE_DRS_DFP_UFP_Send_Swap
	 * states embedded here.
	 */
	/* Request the Protocol Layer to send a DR_Swap Message */
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_DR_SWAP);

	pe[port].sender_response_timer = 0;

	return 0;
}

static int pe_drs_send_swap_run(int port)
{
	int type;
	int cnt;
	int ext;

	/* Wait until message is sent */
	if (pe[port].sender_response_timer == 0 &&
			PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		/* start the SenderResponseTimer */
		pe[port].sender_response_timer =
				get_time().val + PD_T_SENDER_RESPONSE;
	}

	if (pe[port].sender_response_timer == 0)
		return 0;

	/*
	 * Transition to PE_SRC_Ready or PE_SNK_Ready state when:
	 *   1) Or the SenderResponseTimer times out.
	 */
	if (get_time().val > pe[port].sender_response_timer) {
		if (pe[port].power_role == PD_ROLE_SINK)
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		return 0;
	}

	/*
	 * Transition to PE_DRS_Change when:
	 *   1) An Accept Message is received.
	 *
	 * Transition to PE_SRC_Ready or PE_SNK_Ready state when:
	 *   1) A Reject Message is received.
	 *   2) Or a Wait Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0)) {
			if (type == PD_CTRL_ACCEPT) {
				sm_set_state(port, PE_OBJ(port), pe_drs_change);
			} else if ((type == PD_CTRL_REJECT) ||
						(type == PD_CTRL_WAIT)) {
				if (pe[port].power_role == PD_ROLE_SINK)
					sm_set_state(port, PE_OBJ(port),
								pe_snk_ready);
				else
					sm_set_state(port, PE_OBJ(port),
								pe_src_ready);
			}
		}
	}

	return 0;
}

/**
 * PE_PRS_SRC_SNK_Evaluate_Swap
 */
static int pe_prs_src_snk_evaluate_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_src_snk_evaluate_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_src_snk_evaluate_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SRC_SNK_EVALUATE_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SRC_SNK_EVALUATE_SWAP;

	if (!pd_check_power_swap(port)) {
		/* PE_PRS_SRC_SNK_Reject_PR_Swap state embedded here */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	} else {
		pd_request_power_swap(port);
		/* PE_PRS_SRC_SNK_Accept_Swap state embedded here */
		PE_SET_FLAG(port, PE_FLAGS_ACCEPT);
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	}

	return 0;
}

static int pe_prs_src_snk_evaluate_swap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (PE_CHK_FLAG(port, PE_FLAGS_ACCEPT)) {
			PE_CLR_FLAG(port, PE_FLAGS_ACCEPT);

			/*
			 * Power Role Swap OK, transition to
			 * PE_PRS_SRC_SNK_Transition_to_off
			 */
			sm_set_state(port, PE_OBJ(port),
					pe_prs_src_snk_transition_to_off);
		} else {
			/* Message sent, return to PE_SRC_Ready */
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		}
	}

	return 0;
}

/**
 * PE_PRS_SRC_SNK_Transition_To_Off
 */
static int pe_prs_src_snk_transition_to_off(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_src_snk_transition_to_off_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_src_snk_transition_to_off_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SRC_SNK_TRANSITION_TO_OFF\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SRC_SNK_TRANSITION_TO_OFF;

	/* Tell TypeC to swap from Attached.SRC to Attached.SNK */
	tc_prs_src_snk_assert_rd(port);
	pe[port].ps_source_timer =
			get_time().val + PD_POWER_SUPPLY_TURN_OFF_DELAY;
	return 0;
}

static int pe_prs_src_snk_transition_to_off_run(int port)
{
	/* Give time for supply to power off */
	if (get_time().val < pe[port].ps_source_timer)
		return 0;

	/* Wait until Rd is asserted */
	if (get_typec_state_id(port) == TC_ATTACHED_SNK) {
		/* Contract is invalid */
		PE_CLR_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);
		sm_set_state(port, PE_OBJ(port), pe_prs_src_snk_wait_source_on);
	}

	return 0;
}

/**
 * PE_PRS_SRC_SNK_Wait_Sorce_On
 */
static int pe_prs_src_snk_wait_source_on(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_src_snk_wait_source_on_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_src_snk_wait_source_on_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SRC_SNK_WAIT_SOURCE_ON\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SRC_SNK_WAIT_SOURCE_ON;

	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
	pe[port].ps_source_timer = 0;

	return 0;
}

static int pe_prs_src_snk_wait_source_on_run(int port)
{
	int type;
	int cnt;
	int ext;

	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/* Update pe power role */
		pe[port].power_role = tc_get_power_role(port);
		pe[port].ps_source_timer = get_time().val + PD_T_PS_SOURCE_ON;
	}

	/*
	 * Transition to PE_SNK_Startup when:
	 *   1) An PS_RDY Message is received.
	 */
	if (pe[port].ps_source_timer > 0 &&
				PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0) && (type == PD_CTRL_PS_RDY)) {
			tc_pr_swap_complete(port);
			pe[port].ps_source_timer = 0;
			return sm_set_state(port, PE_OBJ(port), pe_snk_startup);
		}
	}

	/*
	 * Transition to ErrorRecovery state when:
	 *   1) The PSSourceOnTimer times out.
	 *   2) PS_RDY not sent after retries.
	 */
	if ((pe[port].ps_source_timer > 0 &&
			get_time().val > pe[port].ps_source_timer) ||
			PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);

		return sm_set_state(port, PE_OBJ(port),
					pe_wait_for_error_recovery);
	}

	return 0;
}

/**
 * PE_PRS_SRC_SNK_Send_Swap
 */
static int pe_prs_src_snk_send_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_src_snk_send_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}
static int pe_prs_src_snk_send_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SRC_SNK_SEND_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SRC_SNK_SEND_SWAP;

	/* Request the Protocol Layer to send a PR_Swap Message. */
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PR_SWAP);

	/* Start the SenderResponseTimer */
	pe[port].sender_response_timer =
				get_time().val + PD_T_SENDER_RESPONSE;

	return 0;
}

static int pe_prs_src_snk_send_swap_run(int port)
{
	int type;
	int cnt;
	int ext;

	/*
	 * Transition to PE_SRC_Ready state when:
	 *   1) Or the SenderResponseTimer times out.
	 */
	if (get_time().val > pe[port].sender_response_timer)
		return sm_set_state(port, PE_OBJ(port), pe_src_ready);

	/*
	 * Transition to PE_PRS_SRC_SNK_Transition_To_Off when:
	 *   1) An Accept Message is received.
	 *
	 * Transition to PE_SRC_Ready state when:
	 *   1) A Reject Message is received.
	 *   2) Or a Wait Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0)) {
			if (type == PD_CTRL_ACCEPT)
				sm_set_state(port, PE_OBJ(port),
					pe_prs_src_snk_transition_to_off);
			else if ((type == PD_CTRL_REJECT) ||
						(type == PD_CTRL_WAIT))
				sm_set_state(port, PE_OBJ(port), pe_src_ready);
		}
	}

	return 0;
}

static int pe_prs_src_snk_send_swap_exit(int port)
{
	/* Clear TX Complete Flag if set */
	PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

	return 0;
}

/**
 * PE_PRS_SNK_SRC_Evaluate_Swap
 */
static int pe_prs_snk_src_evaluate_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_snk_src_evaluate_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_snk_src_evaluate_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SNK_SRC_EVALUATE_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SNK_SRC_EVALUATE_SWAP;

	if (!pd_check_power_swap(port)) {
		/* PE_PRS_SNK_SRC_Reject_Swap state embedded here */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	} else {
		pd_request_power_swap(port);
		/* PE_PRS_SNK_SRC_Accept_Swap state embedded here */
		PE_SET_FLAG(port, PE_FLAGS_ACCEPT);
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	}

	return 0;
}

static int pe_prs_snk_src_evaluate_swap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		if (PE_CHK_FLAG(port, PE_FLAGS_ACCEPT)) {
			PE_CLR_FLAG(port, PE_FLAGS_ACCEPT);

			/*
			 * Accept message sent, transition to
			 * PE_PRS_SNK_SRC_Transition_to_off
			 */
			sm_set_state(port, PE_OBJ(port),
					pe_prs_snk_src_transition_to_off);
		} else {
			/* Message sent, return to PE_SNK_Ready */
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		}
	}

	return 0;
}

/**
 * PE_PRS_SNK_SRC_Transition_To_Off
 */
static int pe_prs_snk_src_transition_to_off(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_snk_src_transition_to_off_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_snk_src_transition_to_off_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SNK_SRC_TRANSITION_TO_OFF\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SNK_SRC_TRANSITION_TO_OFF;

	tc_power_off_snk(port);
	pe[port].ps_source_timer = get_time().val + PD_T_PS_SOURCE_OFF;

	return 0;
}

static int pe_prs_snk_src_transition_to_off_run(int port)
{
	int type;
	int cnt;
	int ext;

	/*
	 * Transition to ErrorRecovery state when:
	 *   1) The PSSourceOffTimer times out.
	 */
	if (get_time().val > pe[port].ps_source_timer) {
		return sm_set_state(port, PE_OBJ(port),
					pe_wait_for_error_recovery);
	}

	/*
	 * Transition to PE_PRS_SNK_SRC_Assert_Rp when:
	 *   1) An PS_RDY Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0) && (type == PD_CTRL_PS_RDY)) {
			sm_set_state(port, PE_OBJ(port),
					pe_prs_snk_src_assert_rp);
		}
	}

	return 0;
}

/**
 * PE_PRS_SNK_SRC_Assert_Rp
 */
static int pe_prs_snk_src_assert_rp(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_snk_src_assert_rp_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_snk_src_assert_rp_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SNK_SRC_ASSERT_RP\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SNK_SRC_ASSERT_RP;

	/* Tell TypeC to swap from Attached.SNK to Attached.SRC */
	tc_prs_snk_src_assert_rp(port);
	return 0;
}

static int pe_prs_snk_src_assert_rp_run(int port)
{
	/* Wait until TypeC is in the Attached.SRC state */
	if (get_typec_state_id(port) == TC_ATTACHED_SRC) {
		/* Contract is invalid now */
		PE_CLR_FLAG(port, PE_FLAGS_EXPLICIT_CONTRACT);
		sm_set_state(port, PE_OBJ(port), pe_prs_snk_src_source_on);
	}

	return 0;
}

/**
 * PE_PRS_SNK_SRC_Source_On
 */
static int pe_prs_snk_src_source_on(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_snk_src_source_on_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_snk_src_source_on_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SNK_SRC_SOURCE_ON\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SNK_SRC_SOURCE_ON;

	/*
	 * VBUS was enabled when the TypeC state machine intered
	 * Attached.SRC state
	 */
	pe[port].ps_source_timer = get_time().val +
					PD_POWER_SUPPLY_TURN_ON_DELAY;

	return 0;
}

static int pe_prs_snk_src_source_on_run(int port)
{
	if (get_time().val < pe[port].ps_source_timer)
		return 0;

	if (pe[port].ps_source_timer > 0) {
		/* update pe power role */
		pe[port].power_role = tc_get_power_role(port);
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PS_RDY);
		pe[port].ps_source_timer = 0;
		return 0;
	}

	/*
	 * Transition to ErrorRecovery state when:
	 *   1) On protocol error
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);
		sm_set_state(port, PE_OBJ(port), pe_wait_for_error_recovery);
		return 0;
	}

	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/* Run swap source timer on entry to pe_src_startup */
		PE_SET_FLAG(port, PE_FLAGS_RUN_SOURCE_START_TIMER);
		tc_pr_swap_complete(port);
		sm_set_state(port, PE_OBJ(port), pe_src_startup);
	}

	return 0;
}

/**
 * PE_PRS_SNK_SRC_Send_Swap
 */
static int pe_prs_snk_src_send_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_prs_snk_src_send_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_prs_snk_src_send_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_PRS_SNK_SRC_SEND_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_PRS_SNK_SRC_SEND_SWAP;

	/* Request the Protocol Layer to send a PR_Swap Message. */
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PR_SWAP);

	/* Start the SenderResponseTimer */
	pe[port].sender_response_timer =
				get_time().val + PD_T_SENDER_RESPONSE;

	return 0;
}

static int pe_prs_snk_src_send_swap_run(int port)
{
	int type;
	int cnt;
	int ext;

	/*
	 * Transition to PE_SNK_Ready state when:
	 *   1) Or the SenderResponseTimer times out.
	 */
	if (get_time().val > pe[port].sender_response_timer)
		return sm_set_state(port, PE_OBJ(port), pe_snk_ready);

	/*
	 * Transition to PE_PRS_SNK_SRC_Transition_to_off when:
	 *   1) An Accept Message is received.
	 *
	 * Transition to PE_SNK_Ready state when:
	 *   1) A Reject Message is received.
	 *   2) Or a Wait Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((ext == 0) && (cnt == 0)) {
			if (type == PD_CTRL_ACCEPT)
				sm_set_state(port, PE_OBJ(port),
					pe_prs_snk_src_transition_to_off);
			else if ((type == PD_CTRL_REJECT) ||
							(type == PD_CTRL_WAIT))
				sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		}
	}

	return 0;
}

static int pe_prs_snk_src_send_swap_exit(int port)
{
	/* Clear TX Complete Flag if set */
	PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

	return 0;
}

/**
 * BIST
 */
static int pe_bist(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_bist_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_bist_entry(int port)
{
	uint32_t *payload = (uint32_t *)emsg[port].buf;
	uint8_t mode = (payload[0] >> 28);

	DEBUG_PRINTF("C%d: PE_BIST mode %d\n", port, mode);

	/* Set state id */
	pe[port].state_id = PE_BIST;

	if (mode == 5) {
		prl_send_ctrl_msg(port, TCPC_TX_BIST_MODE_2, 0);
		pe[port].bist_cont_mode_timer =
					get_time().val + (60 * MSEC);
	} else if (mode == 8) {
		pe[port].bist_cont_mode_timer = 0;
	}

	return 0;
}

static int pe_bist_run(int port)
{
	if (pe[port].bist_cont_mode_timer > 0 &&
			get_time().val > pe[port].bist_cont_mode_timer) {

		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_init_state(port, PE_OBJ(port),
						pe_src_transition_to_default);
		else
			sm_init_state(port, PE_OBJ(port),
						pe_snk_transition_to_default);
	} else {
		/*
		 * We are in test mode and no further Messages except for
		 * GoodCRC Messages in response to received Messages will
		 * be sent.
		 */
		if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED))
			PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);
	}

	return 0;
}

static int pe_bist_exit(int port)
{
	return 0;
}

/**
 * Give_Sink_Cap Message
 */
static int pe_snk_give_sink_cap(int port, enum sm_signal sig)
{
	int ret;

	ret = (*pe_snk_give_sink_cap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_snk_give_sink_cap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_SNK_GIVE_SINK_CAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_SNK_GIVE_SINK_CAP;

	/* Send a Sink_Capabilities Message */
	emsg[port].len = pd_snk_pdo_cnt * 4;
	memcpy(emsg[port].buf, (uint8_t *)pd_snk_pdo, emsg[port].len);
	prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_SINK_CAP);
	return 0;
}

static int pe_snk_give_sink_cap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}
	return 0;
}

/**
 * Wait For Error Recovery
 */
static int pe_wait_for_error_recovery(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_wait_for_error_recovery_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_wait_for_error_recovery_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_WAIT_FOR_ERROR_RECOVERY\n", port);

	/* Set state id */
	pe[port].state_id = PE_WAIT_FOR_ERROR_RECOVERY;

	tc_start_error_recovery(port);
	return 0;
}

static int pe_wait_for_error_recovery_run(int port)
{
	/* Stay here until error recovery is complete */
	return 0;
}

static int pe_wait_for_error_recovery_exit(int port)
{
	return 0;
}

static int pe_handle_custom_vdm_request(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_handle_custom_vdm_request_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_handle_custom_vdm_request_entry(int port)
{
	/* Get the message */
	uint32_t *payload = (uint32_t *)emsg[port].buf;
	int cnt = PD_HEADER_CNT(emsg[port].header);
	int sop = PD_HEADER_GET_SOP(emsg[port].header);
	int rlen = 0;
	uint32_t *rdata;

	DEBUG_PRINTF("C%d: PE_HANDLE_CUSTOM_VDM_REQUEST\n", port);

	/* Set state id */
	pe[port].state_id = PE_HANDLE_CUSTOM_VDM_REQUEST;

	/* This is an Interruptible AMS */
	PE_SET_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);

	rlen = pd_custom_vdm(port, cnt, payload, &rdata);
	if (rlen > 0) {
		emsg[port].len = rlen * 4;
		memcpy(emsg[port].buf, (uint8_t *)rdata, emsg[port].len);
		prl_send_data_msg(port, sop, PD_DATA_VENDOR_DEF);
	}

	return 0;
}

static int pe_handle_custom_vdm_request_run(int port)
{
	/* Wait for ACCEPT, WAIT or Reject message to send. */
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		/*
		 * Message sent. Transition back to
		 * PE_SRC_Ready or PE_SINK_Ready
		 */
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

/**
 * PE_DO_PORT_Discovery
 */
static int pe_do_port_discovery(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_do_port_discovery_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_do_port_discovery_entry(int port)
{
	pe[port].partner_is_cable = PORT;
	pe[port].vdm_cnt = 0;
	return 0;
}

static int pe_do_port_discovery_run(int port)
{
	uint32_t *payload = (uint32_t *)emsg[port].buf;
	struct svdm_amode_data *modep = get_modep(port, PD_VDO_VID(payload[0]));
	int ret = 0;

	DEBUG_PRINTF("C%d: PE_DO_PORT_DISCOVERY\n", port);

	/* Set state id */
	pe[port].state_id = PE_DO_PORT_DISCOVERY;

	if (!PE_CHK_FLAG(port, PE_FLAGS_VDM_REQUEST_NAKED |
					PE_FLAGS_VDM_REQUEST_BUSY)) {
		switch (pe[port].vdm_cmd) {
		case DO_PORT_DISCOVERY_START:
			pe[port].vdm_cmd = CMD_DISCOVER_IDENT;
			pe[port].vdm_data[0] = 0;
			ret = 1;
			break;
		case CMD_DISCOVER_IDENT:
			pe[port].vdm_cmd = CMD_DISCOVER_SVID;
			pe[port].vdm_data[0] = 0;
			ret = 1;
			break;
		case CMD_DISCOVER_SVID:
			pe[port].vdm_cmd = CMD_DISCOVER_MODES;
			ret = dfp_discover_modes(port, pe[port].vdm_data);
			break;
		case CMD_DISCOVER_MODES:
			pe[port].vdm_cmd = CMD_ENTER_MODE;
			pe[port].vdm_data[0] = pd_dfp_enter_mode(port, 0, 0);
			if (pe[port].vdm_data[0])
				ret = 1;
			break;
		case CMD_ENTER_MODE:
			pe[port].vdm_cmd = CMD_DP_STATUS;
			if (modep->opos) {
				ret = modep->fx->status(port,
						pe[port].vdm_data);
				pe[port].vdm_data[0] |=
						PD_VDO_OPOS(modep->opos);
			}
			break;
		case CMD_DP_STATUS:
			pe[port].vdm_cmd = CMD_DP_CONFIG;

			/*
			 * DP status response & UFP's DP attention have same
			 * payload
			 */
			dfp_consume_attention(port, pe[port].vdm_data);
			if (modep && modep->opos)
				ret = modep->fx->config(port,
							pe[port].vdm_data);
			break;
		case CMD_DP_CONFIG:
			if (modep && modep->opos && modep->fx->post_config)
				modep->fx->post_config(port);
			PE_SET_FLAG(port, PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE);
			break;
		case CMD_EXIT_MODE:
			/* Do nothing */
			break;
		case CMD_ATTENTION:
			/* Do nothing */
			break;
		}
	}

	if (PE_CHK_FLAG(port, PE_FLAGS_VDM_REQUEST_NAKED) || ret == 0) {
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	} else {
		PE_CLR_FLAG(port, PE_FLAGS_VDM_REQUEST_BUSY);

		/*
		 * Copy Vendor Defined Message (VDM) Header into
		 * message buffer
		 */
		if (pe[port].vdm_data[0] == 0)
			pe[port].vdm_data[0] = VDO(
					USB_SID_PD,
					1, /* structured */
					VDO_SVDM_VERS(1) | pe[port].vdm_cmd);

		pe[port].vdm_data[0] |= VDO_CMDT(CMDT_INIT);
		pe[port].vdm_data[0] |= VDO_SVDM_VERS(pd_get_vdo_ver(port));

		pe[port].vdm_cnt = ret;
		sm_set_state(port, PE_OBJ(port), pe_vdm_request);
	}

	return 0;
}

/**
 * PE_VDM_REQUEST
 */
static int pe_vdm_request(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vdm_request_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vdm_request_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VDM_REQUEST: %d\n", port, pe[port].vdm_cmd);

	/* Set state id */
	pe[port].state_id = PE_VDM_REQUEST;

	/* This is an Interruptible AMS */
	PE_SET_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);

	/* Copy Vendor Data Objects (VDOs) into message buffer */
	if (pe[port].vdm_cnt > 0) {
		/* Copy data after header */
		memcpy(&emsg[port].buf,
			(uint8_t *)pe[port].vdm_data,
			pe[port].vdm_cnt * 4);
		/* Update len with the number of VDO bytes */
		emsg[port].len = pe[port].vdm_cnt * 4;
	}

	if (pe[port].partner_is_cable) {
		/* Save power and data roles */
		pe[port].saved_power_role = tc_get_power_role(port);
		pe[port].saved_data_role = tc_get_data_role(port);

		prl_send_data_msg(port, TCPC_TX_SOP_PRIME, PD_DATA_VENDOR_DEF);
	} else {
		prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_VENDOR_DEF);
	}

	pe[port].vdm_response_timer = 0;

	return 0;
}

static int pe_vdm_request_run(int port)
{
	if (pe[port].vdm_response_timer == 0 &&
					PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (pe[port].partner_is_cable) {
			/* Restore power and data roles */
			tc_set_power_role(port, pe[port].saved_power_role);
			tc_set_data_role(port, pe[port].saved_data_role);
		}

		/* Start no response timer */
		pe[port].vdm_response_timer =
			get_time().val + PD_T_VDM_SNDR_RSP;
	} else if (PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_PROTOCOL_ERROR);

		if (pe[port].partner_is_cable) {
			/* Restore power and data roles */
			tc_set_power_role(port, pe[port].saved_power_role);
			tc_set_data_role(port, pe[port].saved_data_role);
		}

		/* Fake busy response so we try to send command again */
		PE_SET_FLAG(port, PE_FLAGS_VDM_REQUEST_BUSY);
		if (pe[port].obj.last_state == pe_do_port_discovery)
			sm_set_state(port, PE_OBJ(port), pe_do_port_discovery);
		else if (pe[port].obj.last_state == pe_src_vdm_identity_request)
			sm_set_state(port, PE_OBJ(port),
						pe_src_vdm_identity_request);
		else if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		uint32_t *payload;
		int sop;
		uint8_t type;
		uint8_t cnt;
		uint8_t ext;

		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		/* Get the message */
		payload = (uint32_t *)emsg[port].buf;
		sop = PD_HEADER_GET_SOP(emsg[port].header);
		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);
		ext = PD_HEADER_EXT(emsg[port].header);

		if ((sop == TCPC_TX_SOP || sop == TCPC_TX_SOP_PRIME) &&
			type == PD_DATA_VENDOR_DEF && cnt > 0 && ext == 0) {
			if (PD_VDO_CMDT(payload[0]) == CMDT_RSP_ACK)
				return  sm_set_state(port, PE_OBJ(port),
								pe_vdm_acked);
			else if (PD_VDO_CMDT(payload[0]) == CMDT_RSP_NAK ||
				PD_VDO_CMDT(payload[0]) == CMDT_RSP_BUSY) {
				if (PD_VDO_CMDT(payload[0]) == CMDT_RSP_NAK)
					PE_SET_FLAG(port,
						PE_FLAGS_VDM_REQUEST_NAKED);
				else
					PE_SET_FLAG(port,
						PE_FLAGS_VDM_REQUEST_BUSY);

				/* Return to previous state */
				if (pe[port].obj.last_state ==
							pe_do_port_discovery)
					sm_set_state(port,
					PE_OBJ(port), pe_do_port_discovery);
				else if (pe[port].obj.last_state ==
						pe_src_vdm_identity_request)
					sm_set_state(port, PE_OBJ(port),
						pe_src_vdm_identity_request);
				else if (pe[port].power_role == PD_ROLE_SOURCE)
					sm_set_state(port,
						PE_OBJ(port), pe_src_ready);
				else
					sm_set_state(port,
						PE_OBJ(port), pe_snk_ready);
				return 0;
			}
		}
	}

	if (pe[port].vdm_response_timer > 0 &&
				get_time().val > pe[port].vdm_response_timer) {
		CPRINTF("VDM %s Response Timeout\n",
				pe[port].partner_is_cable ? "Cable" : "Port");

		PE_SET_FLAG(port, PE_FLAGS_VDM_REQUEST_NAKED);

		/* Return to previous state */
		if (pe[port].obj.last_state == pe_do_port_discovery)
			sm_set_state(port, PE_OBJ(port), pe_do_port_discovery);
		else if (pe[port].obj.last_state == pe_src_vdm_identity_request)
			sm_set_state(port, PE_OBJ(port),
						pe_src_vdm_identity_request);
		else if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

static int pe_vdm_request_exit(int port)
{
	PE_CLR_FLAG(port, PE_FLAGS_INTERRUPTIBLE_AMS);
	return 0;
}

static int pe_vdm_acked(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vdm_acked_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vdm_acked_entry(int port)
{
	uint32_t *payload;
	uint8_t vdo_cmd;
	int sop;

	DEBUG_PRINTF("C%d: PE_VDM_ACKED\n", port);

	/* Set state id */
	pe[port].state_id = PE_VDM_ACKED;

	/* Get the message */
	payload = (uint32_t *)emsg[port].buf;
	vdo_cmd = PD_VDO_CMD(payload[0]);
	sop = PD_HEADER_GET_SOP(emsg[port].header);

	if (sop == TCPC_TX_SOP_PRIME) {
		/*
		 * Handle Message From Cable Plug
		 */

		uint32_t vdm_header = payload[0];
		uint32_t id_header = payload[1];
		uint8_t ptype_ufp;

		if (PD_VDO_CMD(vdm_header) == CMD_DISCOVER_IDENT &&
				PD_VDO_SVDM(vdm_header) &&
				PD_HEADER_CNT(emsg[port].header) == 5) {
			ptype_ufp = PD_IDH_PTYPE(id_header);

			switch (ptype_ufp) {
			case IDH_PTYPE_UNDEF:
				break;
			case IDH_PTYPE_HUB:
				break;
			case IDH_PTYPE_PERIPH:
				break;
			case IDH_PTYPE_PCABLE:
				/* Passive Cable Detected */
				pe[port].passive_cable_vdo =
						payload[4];
				break;
			case IDH_PTYPE_ACABLE:
				/* Active Cable Detected */
				pe[port].active_cable_vdo1 =
						payload[4];
				pe[port].active_cable_vdo2 =
						payload[5];
				break;
			case IDH_PTYPE_AMA:
				/*
				 * Alternate Mode Adapter
				 * Detected
				 */
				pe[port].ama_vdo = payload[4];
				break;
			case IDH_PTYPE_VPD:
				/*
				 * VCONN Powered Device
				 * Detected
				 */
				pe[port].vpd_vdo = payload[4];

				/*
				 * If a CTVPD device was not discovered, inform
				 * the Device Policy Manager that the Discover
				 * Identity is done.
				 *
				 * If a CTVPD device is discovered, the Device
				 * Policy Manager will clear the DISC_IDENT flag
				 * set by tc_disc_ident_in_progress.
				 */
				if (pe[port].vpd_vdo < 0 ||
						!VPD_VDO_CTS(pe[port].vpd_vdo))
					tc_disc_ident_complete(port);
				break;
			}
		}
	} else {
		/*
		 * Handle Message From Port Partner
		 */

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		int cnt = PD_HEADER_CNT(emsg[port].header);
		struct svdm_amode_data *modep;

		modep = get_modep(port, PD_VDO_VID(payload[0]));
#endif

		switch (vdo_cmd) {
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
		case CMD_DISCOVER_IDENT:
			dfp_consume_identity(port, cnt, payload);
#ifdef CONFIG_CHARGE_MANAGER
			if (pd_charge_from_device(pd_get_identity_vid(port),
						pd_get_identity_pid(port))) {
				charge_manager_update_dualrole(port,
								CAP_DEDICATED);
			}
#endif
			break;
		case CMD_DISCOVER_SVID:
			dfp_consume_svids(port, cnt, payload);
			break;
		case CMD_DISCOVER_MODES:
			dfp_consume_modes(port, cnt, payload);
			break;
		case CMD_ENTER_MODE:
			break;
		case CMD_DP_STATUS:
			/*
			 * DP status response & UFP's DP attention have same
			 * payload
			 */
			dfp_consume_attention(port, payload);
			break;
		case CMD_DP_CONFIG:
			if (modep && modep->opos && modep->fx->post_config)
				modep->fx->post_config(port);
			break;
		case CMD_EXIT_MODE:
			/* Do nothing */
			break;
#endif
		case CMD_ATTENTION:
			/* Do nothing */
			break;
		default:
			CPRINTF("ERR:CMD:%d\n", vdo_cmd);
		}
	}

	if (!PE_CHK_FLAG(port, PE_FLAGS_DISCOVER_VDM_IDENTITY_DONE)) {
		PE_SET_FLAG(port, PE_FLAGS_DISCOVER_VDM_IDENTITY_DONE);
		sm_set_state(port, PE_OBJ(port), pe_src_vdm_identity_request);
	} else if (!PE_CHK_FLAG(port, PE_FLAGS_DISCOVER_PORT_IDENTITY_DONE))
		sm_set_state(port, PE_OBJ(port), pe_do_port_discovery);
	else if (pe[port].power_role == PD_ROLE_SOURCE)
		sm_set_state(port, PE_OBJ(port), pe_src_ready);
	else
		sm_set_state(port, PE_OBJ(port), pe_snk_ready);

	return 0;
}

/**
 * PE_VDM_Response
 */
static int pe_vdm_response(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vdm_response_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vdm_response_entry(int port)
{
	int ret = 0;
	uint32_t *payload;
	uint8_t vdo_cmd;
	int cmd_type;
	int (*func)(int port, uint32_t *payload) = NULL;

	DEBUG_PRINTF("C%d: PE_VDM_RESPONSE\n", port);

	/* Set state id */
	pe[port].state_id = PE_VDM_RESPONSE;

	/* Get the message */
	payload = (uint32_t *)emsg[port].buf;
	vdo_cmd = PD_VDO_CMD(payload[0]);
	cmd_type = PD_VDO_CMDT(payload[0]);
	payload[0] &= ~VDO_CMDT_MASK;

	if (cmd_type != CMDT_INIT) {
		CPRINTF("ERR:CMDT:%d\n", vdo_cmd);

		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		return 0;
	}

	switch (vdo_cmd) {
	case CMD_DISCOVER_IDENT:
		func = svdm_rsp.identity;
		break;
	case CMD_DISCOVER_SVID:
		func = svdm_rsp.svids;
		break;
	case CMD_DISCOVER_MODES:
		func = svdm_rsp.modes;
		break;
	case CMD_ENTER_MODE:
		func = svdm_rsp.enter_mode;
		break;
	case CMD_DP_STATUS:
		func = svdm_rsp.amode->status;
		break;
	case CMD_DP_CONFIG:
		func = svdm_rsp.amode->config;
		break;
	case CMD_EXIT_MODE:
		func = svdm_rsp.exit_mode;
		break;
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
	case CMD_ATTENTION:
		/*
		 * attention is only SVDM with no response
		 * (just goodCRC) return zero here.
		 */
		dfp_consume_attention(port, payload);
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
		return 0;
#endif
	default:
		CPRINTF("VDO ERR:CMD:%d\n", vdo_cmd);
	}

	if (func) {
		ret = func(port, payload);
		if (ret)
			/* ACK */
			payload[0] = VDO(
				USB_VID_GOOGLE,
				1, /* Structured VDM */
				VDO_SVDM_VERS(pd_get_vdo_ver(port)) |
				VDO_CMDT(CMDT_RSP_ACK) |
				vdo_cmd);
		else if (!ret)
			/* NAK */
			payload[0] = VDO(
				USB_VID_GOOGLE,
				1, /* Structured VDM */
				VDO_SVDM_VERS(pd_get_vdo_ver(port)) |
				VDO_CMDT(CMDT_RSP_NAK) |
				vdo_cmd);
		else
			/* BUSY */
			payload[0] = VDO(
				USB_VID_GOOGLE,
				1, /* Structured VDM */
				VDO_SVDM_VERS(pd_get_vdo_ver(port)) |
				VDO_CMDT(CMDT_RSP_BUSY) |
				vdo_cmd);

		if (ret <= 0)
			ret = 4;
	} else {
		/* not supported : NACK it */
		payload[0] = VDO(
			USB_VID_GOOGLE,
			1, /* Structured VDM */
			VDO_SVDM_VERS(pd_get_vdo_ver(port)) |
			VDO_CMDT(CMDT_RSP_NAK) |
			vdo_cmd);
		ret = 4;
	}

	/* Send ACK, NAK, or BUSY */
	emsg[port].len = ret;
	prl_send_data_msg(port, TCPC_TX_SOP, PD_DATA_VENDOR_DEF);

	return 0;
}

static int pe_vdm_response_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL) ||
			PE_CHK_FLAG(port, PE_FLAGS_PROTOCOL_ERROR)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE |
						PE_FLAGS_PROTOCOL_ERROR);

		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

#ifdef CONFIG_USBC_VCONN

/*
 * PE_VCS_Evaluate_Swap
 */
static int pe_vcs_evaluate_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_evaluate_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_evaluate_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_EVALUATE_SWAP\n", port);

	pe[port].state_id = PE_VCS_EVALUATE_SWAP;

	/*
	 * Request the DPM for an evaluation of the VCONN Swap request.
	 * Note: Ports that are presently the VCONN Source must always
	 * accept a VCONN
	 */

	/*
	 * Transition to the PE_VCS_Accept_Swap state when:
	 *  1) The Device Policy Manager indicates that a VCONN Swap is ok.
	 *
	 * Transition to the PE_VCS_Reject_Swap state when:
	 *  1)  Port is not presently the VCONN Source and
	 *  2) The DPM indicates that a VCONN Swap is not ok or
	 *  3) The DPM indicates that a VCONN Swap cannot be done at this time.
	 */

	/* DPM rejects a VCONN Swap and port is not a VCONN source*/
	if (!tc_check_vconn_swap(port) && tc_is_vconn_src(port) < 1) {
		/* NOTE: PE_VCS_Reject_Swap State embedded here */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_REJECT);
	}
	/* Port is not ready to perform a VCONN swap */
	else if (tc_is_vconn_src(port) < 0) {
		/* NOTE: PE_VCS_Reject_Swap State embedded here */
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_WAIT);
	}
	/* Port is ready to perform a VCONN swap */
	else {
		/* NOTE: PE_VCS_Accept_Swap State embedded here */
		PE_SET_FLAG(port, PE_FLAGS_ACCEPT);
		prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_ACCEPT);
	}

	return 0;
}

static int pe_vcs_evaluate_swap_run(int port)
{
	/* Wait for ACCEPT, WAIT or Reject message to send. */
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		if (PE_CHK_FLAG(port, PE_FLAGS_ACCEPT)) {
			PE_CLR_FLAG(port, PE_FLAGS_ACCEPT);
			/* Accept Message sent and Presently VCONN Source */
			if (tc_is_vconn_src(port))
				sm_set_state(port, PE_OBJ(port),
						pe_vcs_wait_for_vconn_swap);
			/* Accept Message sent and Not presently VCONN Source */
			else
				sm_set_state(port, PE_OBJ(port),
						pe_vcs_turn_on_vconn_swap);
		} else {
			/*
			 * Message sent. Transition back to PE_SRC_Ready or
			 * PE_SINK_Ready
			 */
			if (pe[port].power_role == PD_ROLE_SOURCE)
				sm_set_state(port, PE_OBJ(port), pe_src_ready);
			else
				sm_set_state(port, PE_OBJ(port), pe_snk_ready);

		}
	}

	return 0;
}

/*
 * PE_VCS_Send_Swap
 */
static int pe_vcs_send_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_send_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_send_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_SEND_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_VCS_SEND_SWAP;

	/* Send a VCONN_Swap Message */
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_VCONN_SWAP);

	pe[port].sender_response_timer = 0;

	return 0;
}

static int pe_vcs_send_swap_run(int port)
{
	uint8_t type;
	uint8_t cnt;

	/* Wait until message is sent */
	if (pe[port].sender_response_timer == 0 &&
			PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);
		/* Start the SenderResponseTimer */
		pe[port].sender_response_timer = get_time().val +
						PD_T_SENDER_RESPONSE;
	}

	if (pe[port].sender_response_timer == 0)
		return 0;

	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

		type = PD_HEADER_TYPE(emsg[port].header);
		cnt = PD_HEADER_CNT(emsg[port].header);

		/* Only look at control messages */
		if (cnt == 0) {
			/*
			 * Transition to the PE_VCS_Wait_For_VCONN state when:
			 *   1) Accept Message Received and
			 *   2) The Port is presently the VCONN Source.
			 *
			 * Transition to the PE_VCS_Turn_On_VCONN state when:
			 *   1) Accept Message Received and
			 *   2) The Port is not presently the VCONN Source.
			 */
			if (type == PD_CTRL_ACCEPT) {
				if (tc_is_vconn_src(port))
					sm_set_state(port, PE_OBJ(port),
						pe_vcs_wait_for_vconn_swap);
				else
					sm_set_state(port, PE_OBJ(port),
						pe_vcs_turn_on_vconn_swap);
				return 0;
			}

			/*
			 * Transition back to either the PE_SRC_Ready or
			 * PE_SNK_Ready state when:
			 *   1) SenderResponseTimer Timeout or
			 *   2) Reject message is received or
			 *   3) Wait message Received.
			 */
			if (get_time().val > pe[port].sender_response_timer ||
						type == PD_CTRL_REJECT ||
							type == PD_CTRL_WAIT) {
				if (pe[port].power_role == PD_ROLE_SOURCE)
					sm_set_state(port, PE_OBJ(port),
								pe_src_ready);
				else
					sm_set_state(port, PE_OBJ(port),
								pe_snk_ready);
			}
		}
	}

	return 0;
}

/*
 * PE_VCS_Wait_for_VCONN_Swap
 */
static int pe_vcs_wait_for_vconn_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_wait_for_vconn_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_wait_for_vconn_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_WAIT_FOR_VCONN_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_VCS_WAIT_FOR_VCONN_SWAP;

	/* Start the VCONNOnTimer */
	pe[port].vconn_on_timer = get_time().val + PD_T_VCONN_SOURCE_ON;
	return 0;
}

static int pe_vcs_wait_for_vconn_swap_run(int port)
{
	/*
	 * Transition to the PE_VCS_Turn_Off_VCONN state when:
	 *  1) A PS_RDY Message is received.
	 */
	if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED)) {
		PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);
		/*
		 * PS_RDY message received
		 */
		if ((PD_HEADER_CNT(emsg[port].header) == 0) &&
				(PD_HEADER_TYPE(emsg[port].header) ==
						PD_CTRL_PS_RDY)) {
			sm_set_state(port, PE_OBJ(port),
				pe_vcs_turn_off_vconn_swap);
			return 0;
		}
	}

	/*
	 * Transition to either the PE_SRC_Hard_Reset or
	 * PE_SNK_Hard_Reset state when:
	 *   1) The VCONNOnTimer times out.
	 */
	if (get_time().val > pe[port].vconn_on_timer) {
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_hard_reset);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_hard_reset);
	}

	return 0;
}

/*
 * PE_VCS_Turn_On_VCONN_Swap
 */
static int pe_vcs_turn_on_vconn_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_turn_on_vconn_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_turn_on_vconn_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_TURN_ON_VCONN_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_VCS_TURN_ON_VCONN_SWAP;

	/* Request DPM to turn on VCONN */
	pd_request_vconn_swap_on(port);
	pe[port].timeout = 0;

	return 0;
}

static int pe_vcs_turn_on_vconn_swap_run(int port)
{

	/*
	 * Transition to the PE_VCS_Send_Ps_Rdy state when:
	 *  1) The Port’s VCONN is on.
	 */
	if (pe[port].timeout == 0 &&
			PE_CHK_FLAG(port, PE_FLAGS_VCONN_SWAP_COMPLETE)) {
		PE_CLR_FLAG(port, PE_FLAGS_VCONN_SWAP_COMPLETE);
		pe[port].timeout = get_time().val + PD_VCONN_SWAP_DELAY;
	}

	if (pe[port].timeout > 0 && get_time().val > pe[port].timeout)
		sm_set_state(port, PE_OBJ(port), pe_vcs_send_ps_rdy_swap);

	return 0;
}

/*
 * PE_VCS_Turn_Off_VCONN_Swap
 */
static int pe_vcs_turn_off_vconn_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_turn_off_vconn_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_turn_off_vconn_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_TURN_OFF_VCONN_SWAP\n", port);

	/* Set state id */
	pe[port].state_id = PE_VCS_TURN_OFF_VCONN_SWAP;

	/* Request DPM to turn off VCONN */
	pd_request_vconn_swap_off(port);
	pe[port].timeout = 0;
	return 0;
}

static int pe_vcs_turn_off_vconn_swap_run(int port)
{
	/* Wait for VCONN to turn off */
	if (pe[port].timeout == 0 &&
			PE_CHK_FLAG(port, PE_FLAGS_VCONN_SWAP_COMPLETE)) {
		PE_CLR_FLAG(port, PE_FLAGS_VCONN_SWAP_COMPLETE);
		pe[port].timeout = get_time().val + PD_VCONN_SWAP_DELAY;
	}

	if (pe[port].timeout > 0 && get_time().val > pe[port].timeout) {
		if (pe[port].power_role == PD_ROLE_SOURCE)
			sm_set_state(port, PE_OBJ(port), pe_src_ready);
		else
			sm_set_state(port, PE_OBJ(port), pe_snk_ready);
	}

	return 0;
}

/*
 * PE_VCS_Send_PS_Rdy_Swap
 */
static int pe_vcs_send_ps_rdy_swap(int port, enum sm_signal sig)
{
	int ret;

	ret = (pe_vcs_send_ps_rdy_swap_sig[sig])(port);
	return SM_SUPER(ret, sig, 0);
}

static int pe_vcs_send_ps_rdy_swap_entry(int port)
{
	DEBUG_PRINTF("C%d: PE_VCS_SEND_PS_RDY_SWAP\n", port);

	/* Send a PS_RDY Message */
	prl_send_ctrl_msg(port, TCPC_TX_SOP, PD_CTRL_PS_RDY);

	/* Set state id */
	pe[port].state_id = PE_VCS_SEND_PS_RDY_SWAP;
	pe[port].sub = PE_SUB0;

	return 0;
}

static int pe_vcs_send_ps_rdy_swap_run(int port)
{
	if (PE_CHK_FLAG(port, PRL_TX_RX_SIGNAL)) {
		PE_CLR_FLAG(port, PE_FLAGS_TX_COMPLETE);

		switch (pe[port].sub) {
		case PE_SUB0:
			/*
			 * After a VCONN Swap the VCONN Source needs to reset
			 * the Cable Plug’s Protocol Layer in order to ensure
			 * MessageID synchronization.
			 */
			prl_send_ctrl_msg(port, TCPC_TX_SOP_PRIME,
							PD_CTRL_SOFT_RESET);
			pe[port].sub = PE_SUB1;
			pe[port].timeout = get_time().val + 100*MSEC;
			break;
		case PE_SUB1:
			/* Got ACCEPT or REJECT from Cable Plug */
			if (PE_CHK_FLAG(port, PE_FLAGS_MSG_RECEIVED) ||
					get_time().val > pe[port].timeout) {
				PE_CLR_FLAG(port, PE_FLAGS_MSG_RECEIVED);

				if (pe[port].power_role == PD_ROLE_SOURCE)
					sm_set_state(port, PE_OBJ(port),
								pe_src_ready);
				else
					sm_set_state(port, PE_OBJ(port),
								pe_snk_ready);
			}
			break;
		case PE_SUB2:
			/* Do nothing */
			break;
		}
	}

	return 0;
}
#endif /* CONFIG_USBC_VCONN */

/* Policy Engine utility functions */
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
	int i, uw, mv;
	int ret = 0;
	int cur_uw = 0;
	int prefer_cur;
	const uint32_t *src_caps = pe[port].src_caps;

	int __attribute__((unused)) cur_mv = 0;

	/* max voltage is always limited by this boards max request */
	max_mv = MIN(max_mv, PD_MAX_VOLTAGE_MV);

	/* Get max power that is under our max voltage input */
	for (i = 0; i < pe[port].src_cap_cnt; i++) {
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
			int ma = (src_caps[i] & 0x3FF) * 10;

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

void pd_build_request(int port, uint32_t *rdo, uint32_t *ma, uint32_t *mv,
				enum pd_request_type req_type)
{
	uint32_t pdo;
	int pdo_index, flags = 0;
	int uw;
	int max_or_min_ma;
	int max_or_min_mw;
	int max_vbus;
	int vpd_vbus_dcr;
	int vpd_gnd_dcr;

	if (req_type == PD_REQUEST_VSAFE5V) {
		/* src cap 0 should be vSafe5V */
		pdo_index = 0;
		pdo = pe[port].src_caps[0];
	} else {
		/* find pdo index for max voltage we can request */
		pdo_index = pd_find_pdo_index(port, max_request_mv, &pdo);
	}

	pd_extract_pdo_power(pdo, ma, mv);

	/*
	 * Adjust VBUS current if CTVPD device was detected.
	 */
	if (pe[port].vpd_vdo > 0) {
		max_vbus = VPD_VDO_MAX_VBUS(pe[port].vpd_vdo);
		vpd_vbus_dcr = VPD_VDO_VBUS_IMP(pe[port].vpd_vdo) << 1;
		vpd_gnd_dcr = VPD_VDO_GND_IMP(pe[port].vpd_vdo);

		if (max_vbus > VPD_MAX_VBUS_50V)
			max_vbus = VPD_MAX_VBUS_20V;

		/*
		 * Valid max_vbus values:
		 *   20000 mV
		 *   30000 mV
		 *   40000 mV
		 *   50000 mV
		 */
		max_vbus = 20000 + max_vbus * 10000;
		if (*mv > max_vbus)
			*mv = max_vbus;

		/*
		 * 5000 mA cable: 150 = 750000 / 50000
		 * 3000 mA cable: 250 = 750000 / 30000
		 */
		if (*ma > 3000)
			*ma = 750000 / (150 + vpd_vbus_dcr + vpd_gnd_dcr);
		else
			*ma = 750000 / (250 + vpd_vbus_dcr + vpd_gnd_dcr);

		ccprintf("CTVPD MA: %d\n", *ma);
		ccprintf("CTVPD MV: %d\n", *mv);
	}

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
}

void pd_process_source_cap(int port, int cnt, uint32_t *src_caps)
{
#ifdef CONFIG_CHARGE_MANAGER
	uint32_t ma, mv, pdo;
#endif
	int i;

	pe[port].src_cap_cnt = cnt;
	for (i = 0; i < cnt; i++)
		pe[port].src_caps[i] = *src_caps++;

#ifdef CONFIG_CHARGE_MANAGER
	/* Get max power info that we could request */
	pd_find_pdo_index(port, PD_MAX_VOLTAGE_MV, &pdo);
	pd_extract_pdo_power(pdo, &ma, &mv);
	/* Set max. limit, but apply 500mA ceiling */
	charge_manager_set_ceil(port, CEIL_REQUESTOR_PD, PD_MIN_MA);
	pd_set_input_current_limit(port, ma, mv);
#endif
}

void pd_set_max_voltage(unsigned int mv)
{
	max_request_mv = mv;
}

unsigned int pd_get_max_voltage(void)
{
	return max_request_mv;
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


/* VDM utility functions */
#ifdef CONFIG_USB_PD_ALT_MODE_DFP
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

void pd_dfp_pe_init(int port)
{
	memset(&pe[port].am_policy, 0, sizeof(struct pd_policy));
}

#ifdef CONFIG_USB_PD_ALT_MODE_DFP
static void dfp_consume_identity(int port, int cnt, uint32_t *payload)
{
	int ptype = PD_IDH_PTYPE(payload[VDO_I(IDH)]);
	size_t identity_size = MIN(sizeof(pe[port].am_policy.identity),
				(cnt - 1) * sizeof(uint32_t));

	pd_dfp_pe_init(port);
	memcpy(&pe[port].am_policy.identity, payload + 1, identity_size);

	switch (ptype) {
	case IDH_PTYPE_AMA:
/* Leave vbus ON if the following macro is false */
#if defined(CONFIG_USB_PD_DUAL_ROLE) && defined(CONFIG_USBC_VCONN_SWAP)
		/* Adapter is requesting vconn, try to supply it */
		if (PD_VDO_AMA_VCONN_REQ(payload[VDO_I(AMA)]))
			tc_vconn_on(port);

		/* Only disable vbus if vconn was requested */
		if (PD_VDO_AMA_VCONN_REQ(payload[VDO_I(AMA)]) &&
				!PD_VDO_AMA_VBUS_REQ(payload[VDO_I(AMA)]))
			pd_power_supply_reset(port);
#endif
		break;
	default:
		break;
	}
}

static void dfp_consume_svids(int port, int cnt, uint32_t *payload)
{
	int i;
	uint32_t *ptr = payload + 1;
	int vdo = 1;
	uint16_t svid0, svid1;

	for (i = pe[port].am_policy.svid_cnt;
				i < pe[port].am_policy.svid_cnt + 12; i += 2) {
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
		pe[port].am_policy.svids[i].svid = svid0;
		pe[port].am_policy.svid_cnt++;

		svid1 = PD_VDO_SVID_SVID1(*ptr);
		if (!svid1)
			break;
		pe[port].am_policy.svids[i + 1].svid = svid1;
		pe[port].am_policy.svid_cnt++;
		ptr++;
		vdo++;
	}

	/* TODO(tbroch) need to re-issue discover svids if > 12 */
	if (i && ((i % 12) == 0))
		CPRINTF("ERR:SVID+12\n");
}

static int dfp_discover_modes(int port, uint32_t *payload)
{
	uint16_t svid =
		pe[port].am_policy.svids[pe[port].am_policy.svid_idx].svid;

	if (pe[port].am_policy.svid_idx >= pe[port].am_policy.svid_cnt)
		return 0;

	payload[0] = VDO(svid, 1, CMD_DISCOVER_MODES);

	return 1;
}

static void dfp_consume_modes(int port, int cnt, uint32_t *payload)
{
	int idx = pe[port].am_policy.svid_idx;

	pe[port].am_policy.svids[idx].mode_cnt = cnt - 1;

	if (pe[port].am_policy.svids[idx].mode_cnt < 0) {
		CPRINTF("ERR:NOMODE\n");
	} else {
		memcpy(
		pe[port].am_policy.svids[pe[port].am_policy.svid_idx].mode_vdo,
		&payload[1],
		sizeof(uint32_t) * pe[port].am_policy.svids[idx].mode_cnt);
	}

	pe[port].am_policy.svid_idx++;
}

static int get_mode_idx(int port, uint16_t svid)
{
	int i;

	for (i = 0; i < PD_AMODE_COUNT; i++) {
		if (pe[port].am_policy.amodes[i].fx->svid == svid)
			return i;
	}

	return -1;
}

static struct svdm_amode_data *get_modep(int port, uint16_t svid)
{
	int idx = get_mode_idx(port, svid);

	return (idx == -1) ? NULL : &pe[port].am_policy.amodes[idx];
}

int pd_alt_mode(int port, uint16_t svid)
{
	struct svdm_amode_data *modep = get_modep(port, svid);

	return (modep) ? modep->opos : -1;
}

int allocate_mode(int port, uint16_t svid)
{
	int i, j;
	struct svdm_amode_data *modep;
	int mode_idx = get_mode_idx(port, svid);

	if (mode_idx != -1)
		return mode_idx;

	/* There's no space to enter another mode */
	if (pe[port].am_policy.amode_idx == PD_AMODE_COUNT) {
		CPRINTF("ERR:NO AMODE SPACE\n");
		return -1;
	}

	/* Allocate ...  if SVID == 0 enter default supported policy */
	for (i = 0; i < supported_modes_cnt; i++) {
		if (!&supported_modes[i])
			continue;

		for (j = 0; j < pe[port].am_policy.svid_cnt; j++) {
			struct svdm_svid_data *svidp =
						&pe[port].am_policy.svids[j];

			if ((svidp->svid != supported_modes[i].svid) ||
					(svid && (svidp->svid != svid)))
				continue;

			modep =
		&pe[port].am_policy.amodes[pe[port].am_policy.amode_idx];
			modep->fx = &supported_modes[i];
			modep->data = &pe[port].am_policy.svids[j];
			pe[port].am_policy.amode_idx++;
			return pe[port].am_policy.amode_idx - 1;
		}
	}
	return -1;
}

uint32_t pd_dfp_enter_mode(int port, uint16_t svid, int opos)
{
	int mode_idx = allocate_mode(port, svid);
	struct svdm_amode_data *modep;
	uint32_t mode_caps;

	if (mode_idx == -1)
		return 0;

	modep = &pe[port].am_policy.amodes[mode_idx];

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

	PE_SET_FLAG(port, PE_FLAGS_MODAL_OPERATION);

	/* SVDM to send to UFP for mode entry */
	return VDO(modep->fx->svid, 1, CMD_ENTER_MODE | VDO_OPOS(modep->opos));
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

static void dfp_consume_attention(int port, uint32_t *payload)
{
	uint16_t svid = PD_VDO_VID(payload[0]);
	int opos = PD_VDO_OPOS(payload[0]);
	struct svdm_amode_data *modep = get_modep(port, svid);

	if (!modep || !validate_mode_request(modep, svid, opos))
		return;

	if (modep->fx->attention)
		modep->fx->attention(port, payload);
}
#endif
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
	struct svdm_amode_data *modep = get_modep(port, USB_SID_DISPLAYPORT);
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

int pd_dfp_exit_mode(int port, uint16_t svid, int opos)
{
	struct svdm_amode_data *modep;
	int idx;


	/*
	 * Empty svid signals we should reset DFP VDM state by exiting all
	 * entered modes then clearing state.  This occurs when we've
	 * disconnected or for hard reset.
	 */
	if (!svid) {
		for (idx = 0; idx < PD_AMODE_COUNT; idx++)
			if (pe[port].am_policy.amodes[idx].fx)
				pe[port].am_policy.amodes[idx].fx->exit(port);

		pd_dfp_pe_init(port);
		return 0;
	}

	/*
	 * TODO(crosbug.com/p/33946) : below needs revisited to allow multiple
	 * mode exit.  Additionally it should honor OPOS == 7 as DFP's request
	 * to exit all modes.  We currently don't have any UFPs that support
	 * multiple modes on one SVID.
	 */
	modep = get_modep(port, svid);
	if (!modep || !validate_mode_request(modep, svid, opos))
		return 0;

	/* call DFPs exit function */
	modep->fx->exit(port);

	PE_CLR_FLAG(port, PE_FLAGS_MODAL_OPERATION);

	/* exit the mode */
	modep->opos = 0;

	return 1;
}

uint16_t pd_get_identity_vid(int port)
{
	return PD_IDH_VID(pe[port].am_policy.identity[0]);
}

uint16_t pd_get_identity_pid(int port)
{
	return PD_PRODUCT_PID(pe[port].am_policy.identity[2]);
}


#ifdef CONFIG_CMD_USB_PD_PE
static void dump_pe(int port)
{
	const char * const idh_ptype_names[]  = {
		"UNDEF", "Hub", "Periph", "PCable", "ACable", "AMA",
		"RSV6", "RSV7"};

	int i, j, idh_ptype;
	struct svdm_amode_data *modep;
	uint32_t mode_caps;

	if (pe[port].am_policy.identity[0] == 0) {
		ccprintf("No identity discovered yet.\n");
		return;
	}
	idh_ptype = PD_IDH_PTYPE(pe[port].am_policy.identity[0]);
	ccprintf("IDENT:\n");
	ccprintf("\t[ID Header] %08x :: %s, VID:%04x\n",
				pe[port].am_policy.identity[0],
				idh_ptype_names[idh_ptype],
				pd_get_identity_vid(port));
	ccprintf("\t[Cert Stat] %08x\n", pe[port].am_policy.identity[1]);
	for (i = 2; i < ARRAY_SIZE(pe[port].am_policy.identity); i++) {
		ccprintf("\t");
		if (pe[port].am_policy.identity[i])
			ccprintf("[%d] %08x ", i,
					pe[port].am_policy.identity[i]);
	}
	ccprintf("\n");

	if (pe[port].am_policy.svid_cnt < 1) {
		ccprintf("No SVIDS discovered yet.\n");
		return;
	}

	for (i = 0; i < pe[port].am_policy.svid_cnt; i++) {
		ccprintf("SVID[%d]: %04x MODES:", i,
					pe[port].am_policy.svids[i].svid);
		for (j = 0; j < pe[port].am_policy.svids[j].mode_cnt; j++)
			ccprintf(" [%d] %08x", j + 1,
			 pe[port].am_policy.svids[i].mode_vdo[j]);
		ccprintf("\n");
		modep = get_modep(port, pe[port].am_policy.svids[i].svid);
		if (modep) {
			mode_caps = modep->data->mode_vdo[modep->opos - 1];
			ccprintf("MODE[%d]: svid:%04x caps:%08x\n", modep->opos,
				 modep->fx->svid, mode_caps);
		}
	}
}

static int command_pe(int argc, char **argv)
{
	int port;
	char *e;

	if (argc < 3)
		return EC_ERROR_PARAM_COUNT;

	/* command: pe <port> <subcmd> <args> */
	port = strtoi(argv[1], &e, 10);
	if (*e || port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_ERROR_PARAM2;
	if (!strncasecmp(argv[2], "dump", 4))
		dump_pe(port);

	return EC_SUCCESS;
}

DECLARE_CONSOLE_COMMAND(pe, command_pe,
			"<port> dump",
			"USB PE");
#endif /* CONFIG_CMD_USB_PD_PE */

static int hc_remote_pd_discovery(struct host_cmd_handler_args *args)
{
	const uint8_t *port = args->params;
	struct ec_params_usb_pd_discovery_entry *r = args->response;

	if (*port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	r->vid = pd_get_identity_vid(*port);
	r->ptype = PD_IDH_PTYPE(pe[*port].am_policy.identity[0]);

	/* pid only included if vid is assigned */
	if (r->vid)
		r->pid = PD_PRODUCT_PID(pe[*port].am_policy.identity[2]);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_DISCOVERY,
		hc_remote_pd_discovery,
		EC_VER_MASK(0));

static int hc_remote_pd_get_amode(struct host_cmd_handler_args *args)
{
	struct svdm_amode_data *modep;
	const struct ec_params_usb_pd_get_mode_request *p = args->params;
	struct ec_params_usb_pd_get_mode_response *r = args->response;

	if (p->port >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	/* no more to send */
	if (p->svid_idx >= pe[p->port].am_policy.svid_cnt) {
		r->svid = 0;
		args->response_size = sizeof(r->svid);
		return EC_RES_SUCCESS;
	}

	r->svid = pe[p->port].am_policy.svids[p->svid_idx].svid;
	r->opos = 0;
	memcpy(r->vdo, pe[p->port].am_policy.svids[p->svid_idx].mode_vdo, 24);
	modep = get_modep(p->port, r->svid);

	if (modep)
		r->opos = pd_alt_mode(p->port, r->svid);

	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_GET_AMODE,
	hc_remote_pd_get_amode,
	EC_VER_MASK(0));

#endif /* CONFIG_USB_PD_ALT_MODE_DFP */
