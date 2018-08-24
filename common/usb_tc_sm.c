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
#include "tcpm.h"
#include "usb_pe_sm.h"
#include "usb_prl_sm.h"
#include "usb_sm.h"
#include "version.h"

#undef PD_DEFAULT_STATE
/* Port default state at startup */
#ifdef CONFIG_USB_PD_DUAL_ROLE
#define PD_DEFAULT_STATE(port) ((PD_ROLE_DEFAULT(port) == PD_ROLE_SOURCE) ? \
				tc_state_unattached_src :           \
				tc_state_unattached_snk)
#else
#define PD_DEFAULT_STATE(port) tc_state_unattached_src
#endif

#ifdef CONFIG_COMMON_RUNTIME
#define CPRINTF(format, args...) cprintf(CC_USBPD, format, ## args)
#define CPRINTS(format, args...) cprints(CC_USBPD, format, ## args)

#if 1
#define STATE_NAME_PRINT(n, p) CPRINTS("C%d %s", (p), (n))
#else
#define STATE_NAME_PRINT(n, p)
#endif

#endif

#define SM_FLAGS_PD_ENABLE  (1 << 0)


#define TC_OBJ(port)   (SM_OBJ(tc[port]))

#ifdef CONFIG_USB_PD_DUAL_ROLE
#define DUAL_ROLE_IF_ELSE(port, sink_clause, src_clause) \
	(tc[port].power_role == PD_ROLE_SINK ? (sink_clause) : (src_clause))
#else
#define DUAL_ROLE_IF_ELSE(port, sink_clause, src_clause) (src_clause)
#endif

#define READY_RETURN_STATE(port) DUAL_ROLE_IF_ELSE(port, pd_state_snk_ready, \
							 pd_state_src_ready)

/* Type C supply voltage (mV) */
#define TYPE_C_VOLTAGE	5000 /* mV */

/* Type C default sink current (mA) */
#define TYPE_C_CURRENT  500 /* mA */

#ifdef CONFIG_USB_PD_DUAL_ROLE
/* Port dual-role state */
enum pd_dual_role_states drp_state[CONFIG_USB_PD_PORT_COUNT] = {
	[0 ... (CONFIG_USB_PD_PORT_COUNT - 1)] =
		CONFIG_USB_PD_INITIAL_DRP_STATE
};

#ifdef CONFIG_USB_PD_TRY_SRC
/* Enable variable for Try.SRC states */
static uint8_t pd_try_src_enable;
#endif
#endif

static struct type_c {
	/* struct sm_obj must be first */
	struct sm_obj obj;
	/* event timeout */
	uint64_t evt_timeout;
	/* state machine event */
	int evt;
	/* true if power delivery is enabled */
	uint8_t pd_is_enabled;
	/* Current port power role (SOURCE or SINK) */
	uint8_t power_role;
	/* current port data role (DFP or UFP) */
	uint8_t data_role;
	/* Port polarity : 0 => CC1 is CC line, 1 => CC2 is CC line */
	uint8_t polarity;
	/* port flags, see PD_FLAGS_* */
	uint32_t flags;
	/* port state machine flags */
	uint32_t sm_flags;
	/* Time a port shall wait before it can determine it is attached */
	uint64_t cc_debounce;
	/* Time a Sink port shall wait before it can determine it is detached
	 * due to the potential for USB PD signaling on CC as described in
	 * the state definitions.
	 */
	uint64_t pd_debounce;
	/*
	 * Time for Time a port shall wait before it can determine it is
	 * re-attached during the try-wait process.
	 */
	uint64_t try_wait_debounce;
	/* The cc state */
	enum pd_cc_states cc_state;
	enum pd_cc_states new_cc_state;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	uint64_t next_role_swap;
	typec_current_t typec_curr;
	typec_current_t typec_curr_change;
#endif
} tc[CONFIG_USB_PD_PORT_COUNT];

/* Type-C states */
static void tc_state_disabled(int port, int sig);
static void tc_state_unattached_snk(int port, int sig);
static void tc_state_attached_wait_snk(int port, int sig);
static void tc_state_attached_snk(int port, int sig);
static void tc_state_unattached_src(int port, int sig);
static void tc_state_attached_wait_src(int port, int sig);
static void tc_state_attached_src(int port, int sig);
#ifdef CONFIG_USB_PD_TRY_SRC
static void tc_state_try_src(int port, int sig);
static void tc_state_try_wait_snk(int port, int sig);
#endif
static void tc_state_audio_accessory(int port, int sig);
static void tc_state_unoriented_debug_accessory_src(int port, int sig);
//static void tc_state_oriented_debug_accessory_src(int port, int sig);
static void tc_state_debug_accessory_snk(int port, int sig);

static void set_polarity(int port, int polarity)
{
	tcpm_set_polarity(port, polarity);
#ifdef CONFIG_USBC_PPC_POLARITY
	ppc_set_polarity(port, polarity);
#endif /* defined(CONFIG_USBC_PPC_POLARITY) */
}

#ifdef CONFIG_USBC_VCONN_SWAP
static void pd_request_vconn_swap(int port)
{
}
#endif

#ifdef CONFIG_USBC_VCONN
static void set_vconn(int port, int enable)
{
	if (enable == 0)
		return;
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

static void pd_set_power_role(int port, int role)
{
	tc[port].power_role = role;
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

	/*
	 * Clear this flag to cover case where a TrySrc
	 * mode went from enabled to disabled and trying_source
	 * was active at that time.
	 */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		tc[i].flags &= ~PD_FLAGS_TRY_SRC;
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, pd_update_try_source, HOOK_PRIO_DEFAULT);
#endif

static void pd_set_data_role(int port, int role)
{
	tc[port].data_role = role;

#ifdef CONFIG_USBC_SS_MUX
#ifdef CONFIG_USBC_SS_MUX_DFP_ONLY
	/*
	 * Need to connect SS mux for if new data role is DFP.
	 * If new data role is UFP, then disconnect the SS mux.
	 */
	if (role == PD_ROLE_DFP)
		usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
			tc[port].polarity);
	else
		usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
			tc[port].polarity);
#else
	usb_mux_set(port, TYPEC_MUX_USB, USB_SWITCH_CONNECT,
			tc[port].polarity);
#endif
#endif
	/* Notify TCPC of role update */
	tcpm_set_msg_header(port, tc[port].power_role,
			tc[port].data_role);
}

/**
 * Returns whether the sink has detected a Rp resistor on the other side.
 */
static inline int cc_is_rp(int cc)
{
	return (cc == TYPEC_CC_VOLT_SNK_DEF) || (cc == TYPEC_CC_VOLT_SNK_1_5) ||
	       (cc == TYPEC_CC_VOLT_SNK_3_0);
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

/**
 * Returns the polarity of a Sink.
 */
static inline int get_snk_polarity(int cc1, int cc2)
{
	/* the following assumes:
	 * TYPEC_CC_VOLT_SNK_3_0 > TYPEC_CC_VOLT_SNK_1_5
	 * TYPEC_CC_VOLT_SNK_1_5 > TYPEC_CC_VOLT_SNK_DEF
	 * TYPEC_CC_VOLT_SNK_DEF > TYPEC_CC_VOLT_OPEN
	 */
	return (cc2 > cc1);
}

/* Local convenience method for two method currently always called together. */
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
static void pd_set_drp_toggle(int port, int enable)
{
	tcpm_set_drp_toggle(port, enable);
}
#endif /* CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE */

#ifdef CONFIG_COMMON_RUNTIME
/* Initialize globals based on system state. */
static void pd_init_tasks(void)
{
	static int initialized;

	/* Initialize globals once, for all PD tasks.  */
	if (initialized)
		return;

	initialized = 1;
}
#endif /* CONFIG_COMMON_RUNTIME */

#ifndef CONFIG_USB_PD_TCPC
static int pd_restart_tcpc(int port)
{
	if (board_set_tcpc_power_mode) {
		/* force chip reset */
		board_set_tcpc_power_mode(port, 0);
	}

	tcpm_init(port);
	pe_init(port);

	return 0;
}
#endif

#if defined(CONFIG_CHARGE_MANAGER)
/**
 * Returns type C current limit (mA) based upon cc_voltage (mV).
 */
static typec_current_t get_typec_current_limit(int polarity, int cc1, int cc2)
{
	typec_current_t charge;
	int cc = polarity ? cc2 : cc1;
	int cc_alt = polarity ? cc1 : cc2;

	if (cc == TYPEC_CC_VOLT_SNK_3_0 && cc_alt != TYPEC_CC_VOLT_SNK_1_5)
		charge = 3000;
	else if (cc == TYPEC_CC_VOLT_SNK_1_5)
		charge = 1500;
	else
		charge = 0;

	if (cc_is_rp(cc_alt))
		charge |= TYPEC_CURRENT_DTS_MASK;

	return charge;
}

/*** Public Functions */

int pd_is_vbus_present(int port)
{
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	return tcpm_get_vbus_level(port);
#else
	return pd_snk_is_vbus_provided(port);
#endif
}

/* Return flag for pd state is connected */
int pd_is_connected(int port)
{
	if ((tc[port].obj.task_state == tc_state_attached_snk) ||
			(tc[port].obj.task_state == tc_state_attached_src))
		return 1;
	return 0;
}

void tc_set_vconn(int port, int enable)
{
#ifdef CONFIG_USBC_VCONN
	set_vconn(port, enable);
#endif
}

#ifdef CONFIG_USB_PD_DUAL_ROLE

void pd_request_power_swap(int port)
{
}

#ifdef CONFIG_USBC_VCONN_SWAP
void pd_try_vconn_src(int port)
{
	/*
	 * If we don't currently provide vconn, and we can supply it, send
	 * a vconn swap request.
	 */
	if (!(tc[port].flags & PD_FLAGS_VCONN_ON))
		if (pd_check_vconn_swap(port))
			pd_request_vconn_swap(port);
}
#endif
#endif /* CONFIG_USB_PD_DUAL_ROLE */

/**
 * Signal power request to indicate a charger update that affects the port.
 */
void pd_set_new_power_request(int port)
{
}
#endif /* CONFIG_CHARGE_MANAGER */

void tc_disable_pd(int port)
{
	tc[port].pd_is_enabled = 0;
}

#ifdef CONFIG_COMMON_RUNTIME
void pd_comm_enable(int port, int enable)
{
	if (enable)
		tc[port].sm_flags |= SM_FLAGS_PD_ENABLE;
	else
		tc[port].sm_flags &= ~SM_FLAGS_PD_ENABLE;
}

void pd_set_suspend(int port, int enable)
{
	if (enable)
		set_state(port, TC_OBJ(port), tc_state_disabled);
	else
		set_state(port, TC_OBJ(port), PD_DEFAULT_STATE(port));
}
#endif

int pd_get_role(int port)
{
	return tc[port].data_role;
}

int pd_get_polarity(int port)
{
	return tc[port].polarity;
}

void pd_set_dual_role(int port, enum pd_dual_role_states state)
{
	int i;

	drp_state[port] = state;

#ifdef CONFIG_USB_PD_TRY_SRC
	pd_update_try_source();
#endif

	/* Inform PD tasks of dual role change. */
	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		task_set_event(PD_PORT_TO_TASK_ID(i),
			PD_EVENT_UPDATE_DUAL_ROLE, 0);
}

/* Return true if partner port is known to be PD capable. */
int pd_capable(int port)
{
	return pe_pd_capable(port);
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
void pd_vbus_low(int port)
{
	tc[port].flags &= ~PD_FLAGS_VBUS_NEVER_LOW;
}
#endif

void tc_hard_reset(int port)
{
	pd_set_data_role(port, PD_ROLE_DFP);
}

int tc_get_data_role(int port)
{
	return tc[port].data_role;
}

int tc_get_power_role(int port)
{
	return tc[port].power_role;
}

void pd_request_data_swap(int port)
{
}

#ifdef CONFIG_COMMON_RUNTIME

int pd_is_port_enabled(int port)
{
	if (tc[port].obj.task_state == tc_state_disabled)
		return 0;
	return 1;
}

int pd_fetch_acc_log_entry(int port)
{
	return EC_RES_SUCCESS;
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
void pd_request_source_voltage(int port, int mv)
{
}

void pd_set_external_voltage_limit(int port, int mv)
{
}

void pd_update_contract(int port)
{
}
#endif /* CONFIG_USB_PD_DUAL_ROLE */
#endif /* CONFIG_COMMON_RUNTIME */

void pd_task(void *u)
{
	int port = TASK_ID_TO_PD_PORT(task_get_current());
	int res = 0;
	sm_state this_state;

	tc[port].typec_curr = 0;
	tc[port].typec_curr_change = 0;
#ifdef CONFIG_USB_PD_DUAL_ROLE
	tc[port].next_role_swap = PD_T_DRP_SNK;
#endif /* CONFIG_USB_PD_DUAL_ROLE */

	pd_init_tasks();

	/* Ensure the power supply is in the default state */
	pd_power_supply_reset(port);

	/* Board specific TCPC init */
	board_tcpc_init();

	tcpm_init(port);
	pe_init(port);

	CPRINTS("TCPC p%d init %s", port, res ? "failed" : "ready");
	this_state = res ? tc_state_disabled : PD_DEFAULT_STATE(port);
#ifndef CONFIG_USB_PD_TCPC
	if (!res) {
		struct ec_response_pd_chip_info *info;

		tcpm_get_chip_info(port, 0, &info);
		CPRINTS("TCPC p%d VID:0x%x PID:0x%x DID:0x%x FWV:0x%lx",
			port, info->vendor_id, info->product_id,
			info->device_id, info->fw_version_number);
	}
#endif
	/* Disable TCPC RX until connection is established */
	tcpm_set_rx_enable(port, 0);

#ifdef CONFIG_USBC_SS_MUX
	/* Initialize USB mux to its default state */
	usb_mux_init(port);
#endif

	/* Initialize PD protocol state variables for each port. */
	pd_set_power_role(port, PD_ROLE_DEFAULT(port));

#ifdef CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT
	ASSERT(PD_ROLE_DEFAULT(port) == PD_ROLE_SINK);
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
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	pd_set_drp_toggle(port, 0);
#endif
	init_state(port, TC_OBJ(port), this_state);
	tc[port].evt_timeout = 5*MSEC;
	tc[port].pd_is_enabled = 0;
	tc[port].sm_flags = SM_FLAGS_PD_ENABLE;

	while (1) {
		/* Verify board specific health status : current, voltages... */
		res = pd_board_checks();
		if (res != EC_SUCCESS) {
			set_state(port, TC_OBJ(port),
#ifdef CONFIG_USB_PD_TRY_SRC
				tc_state_unattached_src
#else
				PD_DEFAULT_STATE(port)
#endif
			);
		}

		/* wait for next event/packet or timeout expiration */
		tc[port].evt = task_wait_event(tc[port].evt_timeout);

#ifdef CONFIG_USB_POWER_DELIVERY
		if (tc[port].pd_is_enabled &&
			   (tc[port].obj.task_state == tc_state_attached_src ||
			   tc[port].obj.task_state == tc_state_attached_snk)) {
			tc[port].evt_timeout =
				policy_engine(port, tc[port].evt);
			protocol_layer(port, tc[port].evt);
		}
#endif
		/* send  message to state machine */
		tc[port].obj.task_state(port, RUN_SIG);
	}
}

#ifdef CONFIG_USB_PD_DUAL_ROLE
static void pd_chipset_resume(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
#ifdef CONFIG_CHARGE_MANAGER
		if (charge_manager_get_active_charge_port() != i)
#endif
			tc[i].flags |= PD_FLAGS_CHECK_PR_ROLE |
				       PD_FLAGS_CHECK_DR_ROLE;
		pd_set_dual_role(i, PD_DRP_TOGGLE_ON);
	}

	CPRINTS("PD:S3->S0");
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, pd_chipset_resume, HOOK_PRIO_DEFAULT);

static void pd_chipset_suspend(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		pd_set_dual_role(i, PD_DRP_TOGGLE_OFF);
	CPRINTS("PD:S0->S3");
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, pd_chipset_suspend, HOOK_PRIO_DEFAULT);

static void pd_chipset_startup(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++) {
		pd_set_dual_role(i, PD_DRP_TOGGLE_OFF);
		tc[i].flags |= PD_FLAGS_CHECK_IDENTITY;
	}
	CPRINTS("PD:S5->S3");
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, pd_chipset_startup, HOOK_PRIO_DEFAULT);

static void pd_chipset_shutdown(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_COUNT; i++)
		pd_set_dual_role(i, PD_DRP_FORCE_SINK);
	CPRINTS("PD:S3->S5");
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, pd_chipset_shutdown, HOOK_PRIO_DEFAULT);

#endif /* CONFIG_USB_PD_DUAL_ROLE */

#ifdef CONFIG_COMMON_RUNTIME
#ifdef CONFIG_CMD_PD_DEV_DUMP_INFO
static inline void pd_dev_dump_info(uint16_t dev_id, uint8_t *hash)
{
}
#endif /* CONFIG_CMD_PD_DEV_DUMP_INFO */

static int command_pd(int argc, char **argv)
{
	int port;
	char *e;

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

#if defined(CONFIG_CMD_PD) && defined(CONFIG_USB_PD_DUAL_ROLE)
	/* command: pd <subcmd> <args> */
	if (!strcasecmp(argv[1], "dualrole")) {
		port = strtoi(argv[1], &e, 10);
		if (argc < 3) {
			ccprintf("dual-role toggling: ");
			switch (drp_state[port]) {
			case PD_DRP_TOGGLE_ON:
				ccprintf("on\n");
				break;
			case PD_DRP_TOGGLE_OFF:
				ccprintf("off\n");
				break;
			case PD_DRP_FREEZE:
				ccprintf("freeze\n");
				break;
			case PD_DRP_FORCE_SINK:
				ccprintf("force sink\n");
				break;
			case PD_DRP_FORCE_SOURCE:
				ccprintf("force source\n");
				break;
			}
		} else {
			if (!strcasecmp(argv[2], "on"))
				pd_set_dual_role(port, PD_DRP_TOGGLE_ON);
			else if (!strcasecmp(argv[2], "off"))
				pd_set_dual_role(port, PD_DRP_TOGGLE_OFF);
			else if (!strcasecmp(argv[2], "freeze"))
				pd_set_dual_role(port, PD_DRP_FREEZE);
			else if (!strcasecmp(argv[2], "sink"))
				pd_set_dual_role(port, PD_DRP_FORCE_SINK);
			else if (!strcasecmp(argv[2], "source"))
				pd_set_dual_role(port, PD_DRP_FORCE_SOURCE);
			else
				return EC_ERROR_PARAM3;
		}
		return EC_SUCCESS;
	}
#endif
#ifdef CONFIG_CMD_PD
#ifdef CONFIG_CMD_PD_DEV_DUMP_INFO
	else if (!strncasecmp(argv[1], "rwhashtable", 3)) {
		int i;
		struct ec_params_usb_pd_rw_hash_entry *p;

		for (i = 0; i < RW_HASH_ENTRIES; i++) {
			p = &rw_hash_table[i];
			pd_dev_dump_info(p->dev_id, p->dev_rw_hash);
		}
		return EC_SUCCESS;
	}
#endif /* CONFIG_CMD_PD_DEV_DUMP_INFO */
#ifdef CONFIG_USB_PD_TRY_SRC
	else if (!strncasecmp(argv[1], "trysrc", 6)) {
		int enable;

		if (argc < 2) {
			return EC_ERROR_PARAM_COUNT;
		} else if (argc >= 3) {
			enable = strtoi(argv[2], &e, 10);
			if (*e)
				return EC_ERROR_PARAM3;
			pd_try_src_enable = enable ? 1 : 0;
		}

		ccprintf("Try.SRC %s\n", pd_try_src_enable ? "on" : "off");
		return EC_SUCCESS;
	}
#endif
#endif

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(pd, command_pd,
			"dualrole|dump|rwhashtable"
			"|trysrc [0|1]\n\t<port> "
			"[tx|bist_rx|bist_tx|charger|clock|dev|disable|enable"
			"|soft|hash|hard|ping|state|swap [power|data]|"
			"vdm [ping | curr | vers]]",
			"USB PD");

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
#ifdef CONFIG_USB_PD_DUAL_ROLE
	else if (p->swap == USB_PD_CTRL_SWAP_POWER)
		pd_request_power_swap(p->port);
#ifdef CONFIG_USBC_VCONN_SWAP
	else if (p->swap == USB_PD_CTRL_SWAP_VCONN)
		pd_request_vconn_swap(p->port);
#endif
#endif

	if (args->version == 0) {
		r->enabled = tc[p->port].pd_is_enabled;
		r->role = tc[p->port].power_role;
		r->polarity = tc[p->port].polarity;
		r->state = 0; //tc[p->port].task_state_value;
		args->response_size = sizeof(*r);
	} else {
		r_v1->enabled =
			(tc[p->port].pd_is_enabled ?
				PD_CTRL_RESP_ENABLED_COMMS : 0) |
			(pd_is_connected(p->port) ?
				PD_CTRL_RESP_ENABLED_CONNECTED : 0) |
			(pe_pd_capable(p->port) ?
				PD_CTRL_RESP_ENABLED_PD_CAPABLE : 0);
		r_v1->role =
			(tc[p->port].power_role ? PD_CTRL_RESP_ROLE_POWER : 0) |
			(tc[p->port].data_role ? PD_CTRL_RESP_ROLE_DATA : 0) |
			((tc[p->port].flags & PD_FLAGS_VCONN_ON) ?
				PD_CTRL_RESP_ROLE_VCONN : 0) |
			((tc[p->port].flags & PD_FLAGS_PARTNER_DR_POWER) ?
				PD_CTRL_RESP_ROLE_DR_POWER : 0) |
			((tc[p->port].flags & PD_FLAGS_PARTNER_DR_DATA) ?
				PD_CTRL_RESP_ROLE_DR_DATA : 0) |
			((tc[p->port].flags & PD_FLAGS_PARTNER_USB_COMM) ?
				PD_CTRL_RESP_ROLE_USB_COMM : 0) |
			((tc[p->port].flags & PD_FLAGS_PARTNER_EXTPOWER) ?
				PD_CTRL_RESP_ROLE_EXT_POWERED : 0);
		r_v1->polarity = tc[p->port].polarity;
#if 0
		strzcpy(r_v1->state,
			pd_state_names[tc[p->port].task_state_value],
			sizeof(r_v1->state));
#endif
		args->response_size = sizeof(*r_v1);
	}
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_USB_PD_CONTROL,
		     hc_usb_pd_control,
		     EC_VER_MASK(0) | EC_VER_MASK(1));

static int pd_control(struct host_cmd_handler_args *args)
{
	static int pd_control_disabled[CONFIG_USB_PD_PORT_COUNT];
	const struct ec_params_pd_control *cmd = args->params;
	int enable = 0;

	if (cmd->chip >= CONFIG_USB_PD_PORT_COUNT)
		return EC_RES_INVALID_PARAM;

	/* Always allow disable command */
	if (cmd->subcmd == PD_CONTROL_DISABLE) {
		pd_control_disabled[cmd->chip] = 1;
		return EC_RES_SUCCESS;
	}

	if (pd_control_disabled[cmd->chip])
		return EC_RES_ACCESS_DENIED;

	if (cmd->subcmd == PD_SUSPEND) {
		enable = 0;
	} else if (cmd->subcmd == PD_RESUME) {
		enable = 1;
	} else if (cmd->subcmd == PD_RESET) {
#ifdef HAS_TASK_PDCMD
		board_reset_pd_mcu();
#else
		return EC_RES_INVALID_COMMAND;
#endif
	} else if (cmd->subcmd == PD_CHIP_ON && board_set_tcpc_power_mode) {
		board_set_tcpc_power_mode(cmd->chip, 1);
		return EC_RES_SUCCESS;
	} else {
		return EC_RES_INVALID_COMMAND;
	}

	pd_comm_enable(cmd->chip, enable);
	pd_set_suspend(cmd->chip, !enable);
	return EC_RES_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_PD_CONTROL, pd_control, EC_VER_MASK(0));
#endif /* CONFIG_CMD_PD_CONTROL */

#endif /* CONFIG_COMMON_RUNTIME */

/**
 * The Disabled state is where the port prevents connection from occurring by
 * removing all terminations from the CC pins.
 *
 * The port should transition to the Disabled state from any other state when
 * directed. When the port transitions to the Disabled state from Attached.SNK,
 * it shall keep all terminations on the CC pins removed for a minimum of
 * tErrorRecovery.
 *
 * A port may choose not to support the Disabled state. If the Disabled state is
 * not supported, the port shall be directed to either the Unattached.SNK or
 * Unattached.SRC states after power-on.
 */
static void tc_state_disabled(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("Disabled", port);

		/* Remove VBUS and VCONN */
		pd_power_supply_reset(port);
#ifdef CONFIG_USBC_VCONN
		set_vconn(port, 0);
#endif
		/* Remove the terminations from CC1 and CC2 */
		tcpm_set_cc(port, TYPEC_CC_OPEN);
		break;
	case RUN_SIG:
		task_wait_event(-1);
		break;
	case EXIT_SIG:
#ifndef CONFIG_USB_PD_TCPC
		if (pd_restart_tcpc(port) != 0) {
			CPRINTS("TCPC p%d restart failed!", port);
			break;
		}
#endif
		CPRINTS("TCPC p%d resumed!", port);
		break;
	}
}

/*
 * When in the Unattached.SNK state, the port is waiting to detect the presence
 * of a Source.
 * A port with a dead battery shall enter this state while unpowered.
 */
static void tc_state_unattached_snk(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		/* Ignore role toggle */
		if (tc[port].obj.last_state != tc_state_unattached_src) {
			STATE_NAME_PRINT("Unattached.SNK", port);

			tc[port].flags &= ~PD_FLAGS_RESET_ON_DISCONNECT_MASK;
#ifdef CONFIG_CHARGE_MANAGER
			charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif
			/*
			 * Indicate that the port is disconnected so the board
			 * can restore state from any previous data swap.
			 */
			pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
		}

		/* Swap roles to sink */
		pd_set_power_role(port, PD_ROLE_SINK);

		/*
		 * Both CC1 and CC2 pins shall be independently terminated to
		 * ground through Rd.
		 */
		tcpm_set_cc(port, TYPEC_CC_RD);
		tc[port].next_role_swap =
			get_time().val + PD_T_DRP_SNK;

		break;
	case RUN_SIG:
		/* Check for connection */
		tcpm_get_cc(port, &cc1, &cc2);

		/* Source connection monitoring */
		if (cc1 != TYPEC_CC_VOLT_OPEN || cc2 != TYPEC_CC_VOLT_OPEN)
			set_state(port, TC_OBJ(port),
						tc_state_attached_wait_snk);
		else if	(get_time().val > tc[port].next_role_swap)
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
		break;
	}
}

/*
 * When in the AttachWait.SNK state, the port has detected the SNK.Rp state on
 * at least one of its CC pins and is waiting for VBUS .
 */
static void tc_state_attached_wait_snk(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("AttachedWait.SNK", port);
		tc[port].cc_state = PD_CC_NONE;
		tc[port].new_cc_state = PD_CC_NONE;
		tc[port].cc_debounce = get_time().val + PD_T_CC_DEBOUNCE;
		tc[port].pd_debounce = get_time().val + PD_T_PD_DEBOUNCE;
		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);

		if (cc_is_rp(cc1) && cc_is_rp(cc2)) {
			/* Debug accessory */
			tc[port].new_cc_state = PD_CC_DEBUG_ACC;
		} else if (cc_is_rp(cc1) || cc_is_rp(cc2)) {
			tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
		} else {
			tc[port].new_cc_state = PD_CC_NONE;
		}

		/* Debounce the cc state */
		if (tc[port].new_cc_state != tc[port].cc_state) {
			tc[port].cc_debounce =
					get_time().val + PD_T_CC_DEBOUNCE;
			tc[port].pd_debounce =
					get_time().val + PD_T_PD_DEBOUNCE;
			tc[port].cc_state = tc[port].new_cc_state;
			break;
		}

		if (tc[port].new_cc_state == PD_CC_NONE &&
			get_time().val >= tc[port].pd_debounce) {
			/* We are detached */
#ifdef CONFIG_USB_PD_DUAL_ROLE
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
#else
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
#endif
			break;
		}

		/* Wait for CC debounce */
		if (get_time().val < tc[port].cc_debounce)
			break;
		/* VBUS present */
		if (pd_is_vbus_present(port)) {
			if (tc[port].new_cc_state == PD_CC_DFP_ATTACHED) {
#ifdef CONFIG_USB_PD_TRY_SRC
				if (pd_try_src_enable)
					set_state(port, TC_OBJ(port),
							tc_state_try_src);
				else
#endif
					set_state(port, TC_OBJ(port),
							tc_state_attached_snk);
			} else {
				tc[port].flags |= PD_FLAGS_TS_DTS_PARTNER;
				set_state(port, TC_OBJ(port),
						tc_state_debug_accessory_snk);
			}
			hook_call_deferred(&pd_usb_billboard_deferred_data,
								   PD_T_AME);
			break;
		}
	}
}

/*
 * When in the Attached.SNK state, the port is attached and operating as a Sink.
 * When the port initially enters this state it is also operating as a UFP. The
 * power and data roles can be changed using USB PD commands.
 *
 * A port that entered this state directly from Unattached.SNK due to detecting
 * VBUS shall not determine orientation or availability of higher than Default
 * USB Power and shall not use USB PD.
 */
static void tc_state_attached_snk(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("Attached.SNK", port);

		tcpm_get_cc(port, &cc1, &cc2);

		/* We are attached */
		tc[port].polarity = get_snk_polarity(cc1, cc2);
		set_polarity(port, tc[port].polarity);

		/* initial data role for sink is UFP */
		pd_set_data_role(port, PD_ROLE_UFP);

#if defined(CONFIG_CHARGE_MANAGER)
		tc[port].typec_curr =
			get_typec_current_limit(tc[port].polarity, cc1, cc2);
		typec_set_input_current_limit(port, tc[port].typec_curr,
							TYPE_C_VOLTAGE);
		charge_manager_update_dualrole(port, CAP_DEDICATED);
#endif
		/* ENABLE USB PD STATE MACHINES */
		if (tc[port].sm_flags & SM_FLAGS_PD_ENABLE) {
			tc[port].pd_is_enabled = 1;
			tcpm_set_rx_enable(port, 1);
		}

		break;
	case RUN_SIG:
		if (!pd_is_vbus_present(port)) {
			tc[port].pd_is_enabled = 0;
			tcpm_set_rx_enable(port, 0);
			tcpm_init(port);
			pe_init(port);

			set_state(port, TC_OBJ(port), tc_state_unattached_src);
		}

		if (!(tc[port].sm_flags & SM_FLAGS_PD_ENABLE) &&
						tc[port].pd_is_enabled) {
			tcpm_set_rx_enable(port, 0);
			tc[port].pd_is_enabled = 0;
		} else if ((tc[port].sm_flags & SM_FLAGS_PD_ENABLE) &&
						!tc[port].pd_is_enabled) {
			tc[port].pd_is_enabled = 1;
			tcpm_set_rx_enable(port, 1);
		}

		break;
	}
}

/*
 * When in the Unattached.SRC state, the port is waiting to detect the presence
 * of a Sink or an Accessory.
 */
static void tc_state_unattached_src(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		/* Ignore role toggle */
		if (tc[port].obj.last_state != tc_state_unattached_snk) {
			STATE_NAME_PRINT("Unattached.SRC", port);

			tc[port].flags &= ~PD_FLAGS_RESET_ON_DISCONNECT_MASK;
#if defined(CONFIG_CHARGE_MANAGER)
			charge_manager_update_dualrole(port, CAP_UNKNOWN);
#endif
			/*
			 * Indicate that the port is disconnected so the board
			 * can restore state from any previous data swap.
			 */
			pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
#ifdef CONFIG_USBC_SS_MUX
			usb_mux_set(port, TYPEC_MUX_NONE, USB_SWITCH_DISCONNECT,
							     tc[port].polarity);
#endif
		}

		/* Swap roles to source */
		pd_set_power_role(port, PD_ROLE_SOURCE);

		/*
		 * Both CC1 and CC2 pins shall be independently terminated to
		 * Rp.
		 */
		tcpm_set_cc(port, TYPEC_CC_RP);
		tc[port].next_role_swap =
			get_time().val + PD_T_DRP_SRC;

		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);

		/* Vnc monitoring */
		if ((cc1 == TYPEC_CC_VOLT_RD ||
			cc2 == TYPEC_CC_VOLT_RD) ||
			(cc1 == TYPEC_CC_VOLT_RA &&
			cc2 == TYPEC_CC_VOLT_RA)) {

			tc[port].cc_state = PD_CC_NONE;
			set_state(port, TC_OBJ(port),
					tc_state_attached_wait_src);
		} else if (get_time().val > tc[port].next_role_swap) {
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		}
		break;
	}
}

/*
 * The AttachWait.SRC state is used to ensure that the state of both of the CC1
 * and CC2 pins is stable after a Sink is connected.
 */
static void tc_state_attached_wait_src(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("Attached_Wait.SRC", port);
		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);
		if (cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_RD) {
			/* Debug accessory */
			tc[port].new_cc_state = PD_CC_DEBUG_ACC;
		} else if (cc1 == TYPEC_CC_VOLT_RD ||
					cc2 == TYPEC_CC_VOLT_RD) {
			/* UFP attached */
			tc[port].new_cc_state = PD_CC_UFP_ATTACHED;
		} else if (cc1 == TYPEC_CC_VOLT_RA &&
					cc2 == TYPEC_CC_VOLT_RA) {
			/* Audio accessory */
			tc[port].new_cc_state = PD_CC_AUDIO_ACC;
		} else {
			/* No UFP */
#ifdef CONFIG_USB_PD_DUAL_ROLE
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
#else
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
#endif
			break;
		}

		/* Debounce the cc state */
		if (tc[port].new_cc_state != tc[port].cc_state) {
			tc[port].cc_debounce = get_time().val +
							PD_T_CC_DEBOUNCE;
			tc[port].cc_state = tc[port].new_cc_state;
			break;
		}

		/* Wait for CC debounce */
		if (get_time().val < tc[port].cc_debounce)
			break;

		/* Debounce complete */
		/* UFP is attached */
		if (tc[port].new_cc_state == PD_CC_UFP_ATTACHED) {
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
					USB_SWITCH_DISCONNECT,
					tc[port].polarity);
#endif
			}
			set_state(port, TC_OBJ(port), tc_state_attached_src);
		} else if (tc[port].new_cc_state == PD_CC_DEBUG_ACC) {
			set_state(port, TC_OBJ(port),
				tc_state_unoriented_debug_accessory_src);
		} else {
			set_state(port, TC_OBJ(port), tc_state_audio_accessory);
		}
		break;
	}
}

/*
 * When in the Attached.SRC state, the port is attached and operating as a
 * Source. When the port initially enters this state it is also operating as a
 * DFP. Subsequently, the initial power and data roles can be changed using USB
 * PD commands.
 */
static void tc_state_attached_src(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("Attached.SRC", port);

		tcpm_get_cc(port, &cc1, &cc2);
		/* UFP attached */
		tc[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);
		set_polarity(port, tc[port].polarity);
		/* initial data role for source is DFP */
		pd_set_data_role(port, PD_ROLE_DFP);

		/* ENABLE USB PD STATE MACHINE */
		if (tc[port].sm_flags & SM_FLAGS_PD_ENABLE) {
			tc[port].pd_is_enabled = 1;
			tcpm_set_rx_enable(port, 1);
		}
		break;
	case RUN_SIG:
		/* Source: detect disconnect by monitoring CC */
		tcpm_get_cc(port, &cc1, &cc2);
		if (tc[port].polarity)
			cc1 = cc2;

		if (cc1 == TYPEC_CC_VOLT_OPEN) {
			set_vconn(port, 0);
			pd_power_supply_reset(port);

			pd_set_input_current_limit(port, 0, 0);
			typec_set_input_current_limit(port, 0, 0);
			charge_manager_set_ceil(port, CEIL_REQUESTOR_PD,
							CHARGE_CEIL_NONE);

			tc[port].pd_is_enabled = 0;
			tcpm_set_rx_enable(port, 0);
			tcpm_init(port);
			pe_init(port);

			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
			break;
		}

		if (!(tc[port].sm_flags & SM_FLAGS_PD_ENABLE) &&
						tc[port].pd_is_enabled) {
			tcpm_set_rx_enable(port, 0);
			tc[port].pd_is_enabled = 0;
			break;
		}

		if ((tc[port].sm_flags & SM_FLAGS_PD_ENABLE) &&
						!tc[port].pd_is_enabled) {
			tc[port].pd_is_enabled = 1;
			tcpm_set_rx_enable(port, 1);
		}
		break;
	}
}

#ifdef CONFIG_USB_PD_TRY_SRC
/*
 * When in the Try.SRC state, the port is querying to determine if the port
 * partner supports the Sink role.
 */
static void tc_state_try_src(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("Try.SRC", port);
		/* Swap roles to source */
		pd_set_power_role(port, PD_ROLE_SOURCE);

		/*
		 * Both CC1 and CC2 pins shall be independently terminated to
		 * ground through Rp.
		 */
		tcpm_set_cc(port, TYPEC_CC_RP);
		tcpm_get_cc(port, &cc1, &cc2);

		if ((cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_OPEN) ||
				(cc1 == TYPEC_CC_VOLT_OPEN &&
				 cc2 == TYPEC_CC_VOLT_RD))
			tc[port].cc_state = PD_CC_UFP_ATTACHED;
		else
			tc[port].cc_state = PD_CC_NONE;

		/* Debounce the cc state */
		if (tc[port].cc_state == PD_CC_NONE) {
			if (!pd_is_vbus_present(port))
				tc[port].cc_debounce = get_time().val +
								PD_T_TRY_SRC;
			else
				tc[port].cc_debounce = get_time().val +
								PD_T_TRY_WAIT;
		} else {
			tc[port].try_wait_debounce = get_time().val +
							PD_T_CC_DEBOUNCE;
		}
		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);

		if ((cc1 == TYPEC_CC_VOLT_RD && cc2 == TYPEC_CC_VOLT_OPEN) ||
		     (cc1 == TYPEC_CC_VOLT_OPEN && cc2 == TYPEC_CC_VOLT_RD)) {
			tc[port].new_cc_state = PD_CC_UFP_ATTACHED;
		} else
			tc[port].new_cc_state = PD_CC_NONE;

		/* Debounce the cc state */
		if (tc[port].new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = tc[port].new_cc_state;
			if (tc[port].cc_state == PD_CC_NONE) {
				if (!pd_is_vbus_present(port))
					tc[port].cc_debounce = get_time().val +
							PD_T_TRY_SRC;
				else
					tc[port].cc_debounce = get_time().val +
							PD_T_TRY_WAIT;
			} else {
				tc[port].try_wait_debounce = get_time().val +
							PD_T_CC_DEBOUNCE;
			}
			break;
		}

		/* Wait for CC debounce */
		if (get_time().val < tc[port].cc_debounce)
			break;

		/* Debounce complete */
		/* UFP is attached */
		if (tc[port].new_cc_state == PD_CC_UFP_ATTACHED)
			set_state(port, TC_OBJ(port), tc_state_attached_src);
		else
			set_state(port, TC_OBJ(port), tc_state_try_wait_snk);
		break;
	}
}

/*
 * When in the TryWait.SNK state, the port has failed to become a Source and is
 * waiting to attach as a Sink. Alternatively the port is responding to the Sink
 * being removed while in the Attached.SRC state.
 */
static void tc_state_try_wait_snk(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("TryWait.SNK", port);
		/* Swap roles to sink */
		pd_set_power_role(port, PD_ROLE_SINK);
		tcpm_set_cc(port, TYPEC_CC_RD);
		tcpm_get_cc(port, &cc1, &cc2);

		if (cc_is_rp(cc1) || cc_is_rp(cc2)) {
			tc[port].cc_state = PD_CC_DFP_ATTACHED;
			tc[port].cc_debounce = get_time().val +
						PD_T_CC_DEBOUNCE;
		} else {
			tc[port].cc_state = PD_CC_NONE;
			tc[port].cc_debounce = get_time().val +
						PD_T_DEBOUNCE;
		}
		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);

		if (cc_is_rp(cc1) || cc_is_rp(cc2))
			tc[port].new_cc_state = PD_CC_DFP_ATTACHED;
		else
			tc[port].new_cc_state = PD_CC_NONE;

		/* Debounce the cc state */
		if (tc[port].new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = tc[port].new_cc_state;
			tc[port].cc_debounce = get_time().val +
				(tc[port].cc_state == PD_CC_NONE) ?
				PD_T_DEBOUNCE : PD_T_CC_DEBOUNCE;
			break;
		}

		/* Wait for CC debounce */
		if (get_time().val < tc[port].cc_debounce)
			break;

		/* Disconnection detected */
		if (tc[port].new_cc_state == PD_CC_NONE) {
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		} else {
			/* VBUS present */
			if (pd_is_vbus_present(port)) {
				set_state(port, TC_OBJ(port),
						tc_state_attached_snk);
				hook_call_deferred(
					&pd_usb_billboard_deferred_data,
					PD_T_AME);
			}
		}
		break;
	}
}
#endif

/* The AudioAccessory state is used for the Audio Adapter Accessory Mode. */
static void tc_state_audio_accessory(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("AudioAccessory", port);
#if defined(CONFIG_CHARGE_MANAGER)
		if (pd_is_vbus_present(port)) {
			typec_set_input_current_limit(port, 500,
				TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port,
				CAP_DEDICATED);
		}
#endif
		tc[port].cc_state = PD_CC_AUDIO_ACC;
		tc[port].new_cc_state = PD_CC_AUDIO_ACC;
		break;
	case RUN_SIG:
#if defined(CONFIG_CHARGE_MANAGER)
		if (pd_is_vbus_present(port)) {
			typec_set_input_current_limit(port, 500,
						TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port,
						CAP_DEDICATED);
		} else {
			typec_set_input_current_limit(port, 0, 0);
			charge_manager_set_ceil(port,
				CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
		}
#endif
		tcpm_get_cc(port, &cc1, &cc2);

		if (cc1 == TYPEC_CC_VOLT_OPEN || cc2 == TYPEC_CC_VOLT_OPEN)
			tc[port].new_cc_state = PD_CC_NONE;


		/* Debounce the cc state */
		if (tc[port].new_cc_state != tc[port].cc_state) {
			tc[port].cc_state = tc[port].new_cc_state;
			tc[port].cc_debounce = get_time().val +
						PD_T_CC_DEBOUNCE;
			break;
		}

		/* Wait for CC debounce */
		if ((get_time().val < tc[port].cc_debounce) &&
				tc[port].new_cc_state == PD_CC_NONE) {
			set_state(port, TC_OBJ(port), tc_state_unattached_src);
			break;
		}

		break;
	case EXIT_SIG:
#if defined(CONFIG_CHARGE_MANAGER)
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port,
				CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
#endif
		break;
	}
}

/*
 * The UnorientedDebugAccessory.SRC state is used for the Debug Accessory
 * Mode.
 */
static void tc_state_unoriented_debug_accessory_src(int port, int sig)
{
	int cc1;
	int cc2;

	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("UnorientedDebugAccessory.SRC", port);
		tcpm_get_cc(port, &cc1, &cc2);
		/* We are attached */
		tc[port].polarity = (cc1 != TYPEC_CC_VOLT_RD);

		/* Enable VBUS */
		pd_set_power_supply_ready(port);

		/* SETUP FOR UNORIENTED DEBUG */

		break;
	case RUN_SIG:
		tcpm_get_cc(port, &cc1, &cc2);
		if (tc[port].polarity)
			cc1 = cc2;
		if (cc1 == TYPEC_CC_VOLT_OPEN)
			set_state(port, TC_OBJ(port), PD_DEFAULT_STATE(port));
		break;
	case EXIT_SIG:
		pd_power_supply_reset(port);
		break;
	}
}

#if 0
/* The OrientedDebugAccessory.SRC state is used for the Debug Accessory Mode */
static void tc_state_oriented_debug_accessory_src(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("OrientedDebugAccessory.SRC", port);
		break;
	case RUN_SIG:
		break;
	case EXIT_SIG:
		break;
	}
}
#endif

/* The DebugAccessory.SNK state is used for the Debug Accessory Mode */
static void tc_state_debug_accessory_snk(int port, int sig)
{
	switch (sig) {
	case ENTRY_SIG:
		STATE_NAME_PRINT("DebugAccessory.SNK", port);

		/* SETUP FOR DEBUG ACCESSORY */

		break;
	case RUN_SIG:
		if (!pd_is_vbus_present(port))
			set_state(port, TC_OBJ(port), tc_state_unattached_snk);
		break;
	}
}

