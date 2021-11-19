/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USB Type-C SNK with DebugAccessory
 *   See Figure 4-13 in Release 2.1 of USB Type-C Spec.
 */
#include <logging/log.h>
#include "usb_tc_sm.h"

LOG_MODULE_REGISTER(usb_typec_sm, CONFIG_USB_TYPEC_SM_LOG_LEVEL);


#define TC_SET_FLAG(port, flag) atomic_or(&tc[port].flags, (flag))
#define TC_CLR_FLAG(port, flag) atomic_clear_bits(&tc[port].flags, (flag))
#define TC_CHK_FLAG(port, flag) (tc[port].flags & (flag))

/* Type-C Layer Flags */
/* Flag to note request to power off sink */
#define TC_FLAGS_POWER_OFF_SNK          BIT(0)
/* Flag to note request from pd_set_suspend to enter TC_DISABLED state */
#define TC_FLAGS_REQUEST_SUSPEND        BIT(1)
/* Flag to note we are in TC_DISABLED state */
#define TC_FLAGS_SUSPENDED              BIT(2)
/* Flag for asynchronous call to request Error Recovery */
#define TC_FLAGS_REQUEST_ERROR_RECOVERY	BIT(3)

/* For checking flag_bit_names[] array */
#define TC_FLAGS_COUNT			4

/* List of all TypeC-level states */
enum usb_tc_state {
	/* Super States */
	TC_CC_OPEN,
	TC_CC_RD,
	/* Normal States */
	TC_DISABLED,
	TC_ERROR_RECOVERY,
	TC_UNATTACHED_SNK,
	TC_ATTACH_WAIT_SNK,
	TC_ATTACHED_SNK,
#if (INCLUDE_USB_TC_DBGACC_OPTION)
	TC_DEBUG_ACCESSORY_SNK,
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */

	TC_STATE_COUNT
};
/* Forward declare the full list of states. This is indexed by usb_tc_state */
static const struct usb_state tc_states[];

/*
 * Helper Macro to determine if the machine is in state
 * TC_ATTACHED_SNK
 */
#if (INCLUDE_USB_TC_DBGACC_OPTION)
#define IS_ATTACHED_SNK(port)						\
	((get_state_tc(port) == TC_ATTACHED_SNK) ||			\
	 (get_state_tc(port) == TC_DEBUG_ACCESSORY_SNK))
#else
#define IS_ATTACHED_SNK(port)						\
	(get_state_tc(port) == TC_ATTACHED_SNK)
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */

/*
 * We will use DEBUG LABELS if we will be able to print (COMMON RUNTIME)
 * and either CONFIG_USB_PD_DEBUG_LEVEL is not defined (no override) or
 * we are overriding and the level is not DISABLED.
 *
 * If we can't print or the CONFIG_USB_PD_DEBUG_LEVEL is defined to be 0
 * then the DEBUG LABELS will be removed from the build.
 */
#if (!defined(CONFIG_USB_PD_DEBUG_LEVEL) || (CONFIG_USB_PD_DEBUG_LEVEL > 0))
#define INCLUDE_USB_TC_DEBUG_LABELS
#endif

/* List of human readable state names for console debugging */
__maybe_unused static __const_data const char * const tc_state_names[] = {
#ifdef INCLUDE_USB_TC_DEBUG_LABELS
	[TC_DISABLED] = "Disabled",
	[TC_ERROR_RECOVERY] = "ErrorRecovery",
	[TC_UNATTACHED_SNK] = "Unattached.SNK",
	[TC_ATTACH_WAIT_SNK] = "AttachWait.SNK",
	[TC_ATTACHED_SNK] = "Attached.SNK",
#if (INCLUDE_USB_TC_DBGACC_OPTION)
	[TC_DEBUG_ACCESSORY_SNK] = "DebugAccessory.SNK"
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */

	/* Super States */
	[TC_CC_OPEN] = "SS:CC_OPEN",
	[TC_CC_RD] = "SS:CC_RD",

	[TC_STATE_COUNT] = "",
#endif
};

/* Debug log level - higher number == more log */
#ifdef CONFIG_USB_PD_DEBUG_LEVEL
static const enum debug_level tc_debug_level = CONFIG_USB_PD_DEBUG_LEVEL;
#else
static enum debug_level tc_debug_level = DEBUG_LEVEL_1;
#endif

static struct type_c {
	/* state machine context */
	struct sm_ctx ctx;
	/* current port data role (UFP or Disconnected) */
	enum pd_data_role data_role;
	/* Port polarity */
	enum tcpc_cc_polarity polarity;
	/* port flags, see TC_FLAGS_* */
	uint32_t flags;
	/* The cc state */
	enum pd_cc_states cc_state;
	/* Tasks to notify after TCPC has been reset */
	int tasks_waiting_on_reset;
	/* Tasks preventing TCPC from entering low power mode */
	int tasks_preventing_lpm;
	/* Voltage on CC pin */
	enum tcpc_cc_voltage_status cc_voltage;
	/* Type-C current */
	typec_current_t typec_curr;
	/* Type-C current change */
	typec_current_t typec_curr_change;

	/* Selected TCPC CC/Rp values */
	enum tcpc_cc_pull select_cc_pull;
	enum tcpc_rp_value select_current_limit_rp;
	enum tcpc_rp_value select_collision_rp;
} tc[CONFIG_USB_PD_PORT_MAX_COUNT];

/* Forward declare common, private functions */
static __maybe_unused int reset_device_and_notify(int port);
static void sink_power_sub_states(int port);

/* Forward declare common, private functions */
static void set_state_tc(const int port, const enum usb_tc_state new_state);
test_export_static enum usb_tc_state get_state_tc(const int port);

static void sink_stop_drawing_current(int port);

/*
 * Public Functions
 */

static void tc_detached(int port)
{
	if (INCLUDE_USB_TC_DBGACC_OPTION)
		tcpm_debug_accessory(port, 0);

	if (IS_ENABLED(CONFIG_USB_PRL_SM))
		prl_set_default_pd_revision(port);

	/* Clear any mux connection on detach */
	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		usb_mux_set(port, USB_PD_MUX_NONE,
			    USB_SWITCH_DISCONNECT, tc[port].polarity);
}

const char *tc_get_current_state(int port)
{
	if (IS_ENABLED(INCLUDE_USB_TC_DEBUG_LABELS))
		return tc_state_names[get_state_tc(port)];
	else
		return "";
}

uint32_t tc_get_flags(int port)
{
	return tc[port].flags;
}

int tc_is_attached_snk(int port)
{
	return IS_ATTACHED_SNK(port);
}

void tc_snk_power_off(int port)
{
	if (IS_ATTACHED_SNK(port)) {
		TC_SET_FLAG(port, TC_FLAGS_POWER_OFF_SNK);
		sink_stop_drawing_current(port);
	}
}

/* Set what role the partner is right now, for the PPC and OCP module */
static void tc_set_partner_role(int port, enum ppc_device_role role)
{
	if (IS_ENABLED(CONFIG_USBC_PPC))
		ppc_dev_is_connected(port, role);

	if (IS_ENABLED(CONFIG_USBC_OCP)) {
		usbc_ocp_snk_is_connected(port, role == PPC_DEV_SNK);
		/*
		 * Clear the overcurrent event counter
		 * since we've detected a disconnect.
		 */
		if (role == PPC_DEV_DISCONNECTED)
			usbc_ocp_clear_event_counter(port);
	}
}

/*
 * Depending on the load on the processor and the tasks running
 * it can take a while for the task associated with this port
 * to run.  So build in 1ms delays, for up to 300ms, to wait for
 * the suspend to actually happen.
 */
#define SUSPEND_SLEEP_DELAY	1
#define SUSPEND_SLEEP_RETRIES	300

void pd_set_suspend(int port, int suspend)
{
	if (pd_is_port_enabled(port) == !suspend)
		return;

	/* Track if we are suspended or not */
	if (suspend) {
		int wait = 0;

		TC_SET_FLAG(port, TC_FLAGS_REQUEST_SUSPEND);

		/*
		 * Avoid deadlock when running from task
		 * which we are going to suspend
		 */
		if (PD_PORT_TO_TASK_ID(port) == task_get_current())
			return;

		task_wake(PD_PORT_TO_TASK_ID(port));

		/* Sleep this task if we are not suspended */
		while (pd_is_port_enabled(port)) {
			if (++wait > SUSPEND_SLEEP_RETRIES) {
				LOG_WARN("C%d: NOT SUSPENDED after %dms\n",
					port, wait * SUSPEND_SLEEP_DELAY);
				return;
			}
			msleep(SUSPEND_SLEEP_DELAY);
		}
	} else {
		TC_CLR_FLAG(port, TC_FLAGS_REQUEST_SUSPEND);
		task_wake(PD_PORT_TO_TASK_ID(port));
	}
}

void pd_set_error_recovery(int port)
{
	TC_SET_FLAG(port, TC_FLAGS_REQUEST_ERROR_RECOVERY);
}

int pd_is_port_enabled(int port)
{
	/*
	 * Checking get_state_tc(port) from another task isn't safe since it
	 * can return TC_DISABLED before tc_cc_open_entry and tc_disabled_entry
	 * are complete. So check TC_FLAGS_SUSPENDED instead.
	 */
	return !TC_CHK_FLAG(port, TC_FLAGS_SUSPENDED);
}

int pd_fetch_acc_log_entry(int port)
{
	return EC_RES_SUCCESS;
}

enum tcpc_cc_polarity pd_get_polarity(int port)
{
	return tc[port].polarity;
}

enum pd_data_role pd_get_data_role(int port)
{
	return tc[port].data_role;
}

enum pd_cc_states pd_get_task_cc_state(int port)
{
	return tc[port].cc_state;
}

uint8_t pd_get_task_state(int port)
{
	return get_state_tc(port);
}

const char *pd_get_task_state_name(int port)
{
	return tc_get_current_state(port);
}

int pd_is_connected(int port)
{
	return IS_ATTACHED_SNK(port);
}

bool pd_is_disconnected(int port)
{
	return !pd_is_connected(port);
}

static void bc12_role_change_handler(int port, enum pd_data_role prev_data_role,
	enum pd_data_role data_role)
{
	int event = 0;
	int task_id = USB_CHG_PORT_TO_TASK_ID(port);
	bool role_changed = (data_role != prev_data_role);

	if (!IS_ENABLED(CONFIG_BC12_DETECT_DATA_ROLE_TRIGGER))
		return;

	/* Get the data role of our device */
	switch (data_role) {
	case PD_ROLE_UFP:
		/* Only trigger BC12 detection on a role change */
		if (role_changed)
			event = USB_CHG_EVENT_DR_UFP;
		break;
	case PD_ROLE_DISCONNECTED:
		event = USB_CHG_EVENT_CC_OPEN;
		break;
	default:
		return;
	}

	if (event)
		task_set_event(task_id, event);
}

/*
 * TCPC CC/Rp management
 */
static void typec_select_pull(int port, enum tcpc_cc_pull pull)
{
	tc[port].select_cc_pull = pull;
}
void typec_select_src_current_limit_rp(int port, enum tcpc_rp_value rp)
{
	tc[port].select_current_limit_rp = rp;
}
__overridable int typec_get_default_current_limit_rp(int port)
{
	return CONFIG_USB_PD_PULLUP;
}
static enum tcpc_rp_value typec_get_active_select_rp(int port)
{
	return tc[port].select_current_limit_rp;
}
int typec_update_cc(int port)
{
	int rv;
	enum tcpc_cc_pull pull = tc[port].select_cc_pull;
	enum tcpc_rp_value rp = typec_get_active_select_rp(port);

	rv = tcpm_select_rp_value(port, rp);
	if (rv)
		return rv;

	return tcpm_set_cc(port, pull);
}

void tc_start_error_recovery(int port)
{
	assert(port == TASK_ID_TO_PD_PORT(task_get_current()));

	/*
	 *   The port should transition to the ErrorRecovery state
	 *   from any other state when directed.
	 */
	set_state_tc(port, TC_ERROR_RECOVERY);
}

static void restart_tc_sm(int port, enum usb_tc_state start_state)
{
	int res;

	/* Clear flags before we transitions states */
	tc[port].flags = 0;

	res = tcpm_init(port);

	LOG_PRINTK("C%d: TCPC init %s\n", port, res ? "failed" : "ready");

	/*
	 * Update the Rp Value. We don't need to update CC lines though as that
	 * happens in below set_state transition.
	 */
	typec_select_src_current_limit_rp(port,
		typec_get_default_current_limit_rp(port));

	/* Disable if restart failed, otherwise start in default state. */
	set_state_tc(port, res ? TC_DISABLED : start_state);

	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		/* Initialize USB mux to its default state */
		usb_mux_init(port);

	if (IS_ENABLED(CONFIG_USBC_PPC)) {
		/*
		 * Wait to initialize the PPC after tcpc, which sets
		 * the correct Rd values; otherwise the TCPC might
		 * not be pulling the CC lines down when the PPC connects the
		 * CC lines from the USB connector to the TCPC cause the source
		 * to drop Vbus causing a brown out.
		 */
		ppc_init(port);
	}

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		/*
		 * Only initialize PD supplier current limit to 0.
		 * Defer initializing type-C supplier current limit
		 * to Unattached.SNK or Attached.SNK.
		 */
		pd_set_input_current_limit(port, 0, 0);
		charge_manager_update_dualrole(port, CAP_UNKNOWN);
	}

	/*
	 * PD r3.0 v2.0, ss6.2.1.1.5:
	 * After a physical or logical (USB Type-C Error Recovery) Attach, a
	 * Port discovers the common Specification Revision level between itself
	 * and its Port Partner and/or the Cable Plug(s), and uses this
	 * Specification Revision level until a Detach, Hard Reset or Error
	 * Recovery happens.
	 *
	 * This covers the Error Recovery case, because TC_ERROR_RECOVERY
	 * reinitializes the TC state machine. This also covers the implicit
	 * case when PD is suspended and resumed or when the state machine is
	 * first initialized.
	 */
	if (IS_ENABLED(CONFIG_USB_PRL_SM))
		prl_set_default_pd_revision(port);
}

void tc_state_init(int port)
{
	enum usb_tc_state first_state;

	/* For test builds, replicate static initialization */
	if (IS_ENABLED(TEST_BUILD)) {
		int i;

		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i)
			memset(&tc[i], 0, sizeof(tc[i]));
	}

	/* If port is not available, there is nothing to initialize */
	if (port >= board_get_usb_pd_port_count()) {
		tc_enable_pd(port, 0);
		TC_SET_FLAG(port, TC_FLAGS_REQUEST_SUSPEND);
		return;
	}

	/*
	 * We are going to apply CC open (start with ErrorRecovery state)
	 * unless there is something which forbids us to do that (one of
	 * conditions below is true)
	 */
	first_state = TC_ERROR_RECOVERY;

	/*
	 * If we just lost power, don't apply CC open. Otherwise we would boot
	 * loop, and if this is a fresh power on, then we know there isn't any
	 * stale PD state as well.
	 */
	if (system_get_reset_flags() &
	    (EC_RESET_FLAG_BROWNOUT | EC_RESET_FLAG_POWER_ON))
		first_state = TC_UNATTACHED_SNK;

	/*
	 * If this is non-EFS2 device, battery is not present and EC RO doesn't
	 * keep power-on reset flag after reset caused by H1, then don't apply
	 * CC open because it will cause brown out.
	 *
	 * Please note that we are checking if CONFIG_BOARD_RESET_AFTER_POWER_ON
	 * is defined now, but actually we need to know if it was enabled in
	 * EC RO! It was assumed that if CONFIG_BOARD_RESET_AFTER_POWER_ON is
	 * defined now it was defined in EC RO too.
	 */
	if (!IS_ENABLED(CONFIG_BOARD_RESET_AFTER_POWER_ON) &&
	    !IS_ENABLED(CONFIG_VBOOT_EFS2) && IS_ENABLED(CONFIG_BATTERY) &&
	    (battery_is_present() == BP_NO))
		first_state = TC_UNATTACHED_SNK;

	/*
	 * Initilialize the timers used in this TC state machine
	 *	CC_DEBOUNCE and TIMEOUT
	 */
	k_timer_init(&tc[port].timer_cc_debounce, NULL, NULL);
	k_timer_init(&tc[port].timer_timeout, NULL, NULL);

#ifdef CONFIG_USB_PD_TCPC_BOARD_INIT
	/* Board specific TCPC init */
	board_tcpc_init();
#endif

	/*
	 * Start with ErrorRecovery state if we can to put us in
	 * a clean state from any previous boots.
	 */
	restart_tc_sm(port, first_state);
}

enum pd_cable_plug tc_get_cable_plug(int port)
{
	/*
	 * Messages sent by this state machine are always from a DFP/UFP,
	 * i.e. the chromebook.
	 */
	return PD_PLUG_FROM_DFP_UFP;
}

uint8_t tc_get_polarity(int port)
{
	return tc[port].polarity;
}

/*
 * Private Functions
 */

/* Set the TypeC state machine to a new state. */
static void set_state_tc(const int port, const enum usb_tc_state new_state)
{
	assert(port == TASK_ID_TO_PD_PORT(task_get_current()));

	set_state(port, &tc[port].ctx, &tc_states[new_state]);
}

/* Get the current TypeC state. */
test_export_static enum usb_tc_state get_state_tc(const int port)
{
	/* Default to returning TC_STATE_COUNT if no state has been set */
	if (tc[port].ctx.current == NULL)
		return TC_STATE_COUNT;
	else
		return tc[port].ctx.current - &tc_states[0];
}

/* Get the previous TypeC state. */
static enum usb_tc_state get_last_state_tc(const int port)
{
	return tc[port].ctx.previous - &tc_states[0];
}

static void print_current_state(const int port)
{
	if (IS_ENABLED(INCLUDE_USB_TC_DEBUG_LABELS)) {
		if (tc_debug_level > 1) {
			LOG_INF("C%d: %s\n", port,
				tc_state_names[get_state_tc(port)]);
		}
	} else {
		LOG_PRINTK("C%d: tc-st%d\n", port, get_state_tc(port));
	}
}

void tc_event_check(int port, int evt)
{
	if (evt & PD_EVENT_TCPC_RESET)
		reset_device_and_notify(port);

	if (evt & PD_EVENT_RX_HARD_RESET)
		pd_execute_hard_reset(port);
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

void tc_set_data_role(int port, enum pd_data_role role)
{
	enum pd_data_role prev_data_role;

	prev_data_role = tc[port].data_role;
	tc[port].data_role = role;

	if (IS_ENABLED(CONFIG_USBC_SS_MUX))
		set_usb_mux_with_current_data_role(port);

	/*
	 * For BC1.2 detection that is triggered on data role change events
	 * instead of VBUS changes, need to set an event to wake up the USB_CHG
	 * task and indicate the current data role.
	 */
	bc12_role_change_handler(port, prev_data_role, tc[port].data_role);

	/* Notify TCPC of role update */
	tcpm_set_msg_header(port, PD_ROLE_SINK, tc[port].data_role);
}

static void sink_stop_drawing_current(int port)
{
	pd_set_input_current_limit(port, 0, 0);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		typec_set_input_current_limit(port, 0, 0);
		charge_manager_set_ceil(port,
				CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
	}
}

static __maybe_unused int reset_device_and_notify(int port)
{
	int rv;
	int task, waiting_tasks;

	/* This should only be called from the PD task */
	assert(port == TASK_ID_TO_PD_PORT(task_get_current()));

	rv = tcpm_init(port);
	tc_start_event_loop(port);

	LOG_PRINTK("C%d: TCPC init %s\n", port, rv ? "failed!" : "ready");

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
	atomic_clear_bits(task_get_event_bitmap(task_get_current()),
			  PD_EVENT_TCPC_RESET);

	waiting_tasks = atomic_clear(&tc[port].tasks_waiting_on_reset);

	/* Wake up all waiting tasks. */
	while (waiting_tasks) {
		task = __fls(waiting_tasks);
		waiting_tasks &= ~BIT(task);
		task_set_event(task, TASK_EVENT_PD_AWAKE);
	}

	return rv;
}

static void sink_power_sub_states(int port)
{
	enum tcpc_cc_voltage_status cc1, cc2, cc;
	enum tcpc_cc_voltage_status new_cc_voltage;

	tcpm_get_cc(port, &cc1, &cc2);

	cc = polarity_rm_dts(tc[port].polarity) ? cc2 : cc1;

	if (cc == TYPEC_CC_VOLT_RP_DEF)
		new_cc_voltage = TYPEC_CC_VOLT_RP_DEF;
	else if (cc == TYPEC_CC_VOLT_RP_1_5)
		new_cc_voltage = TYPEC_CC_VOLT_RP_1_5;
	else if (cc == TYPEC_CC_VOLT_RP_3_0)
		new_cc_voltage = TYPEC_CC_VOLT_RP_3_0;
	else
		new_cc_voltage = TYPEC_CC_VOLT_OPEN;

	/* Debounce the cc state */
	if (new_cc_voltage != tc[port].cc_voltage) {
		tc[port].cc_voltage = new_cc_voltage;
		k_timer_start(&tc[port].timer_cc_debounce,
			      K_MSEC(TC_TIMER_CC_DEBOUNCE),
			      K_NO_WAIT);
		return;
	}

	/* If the timer has expired, handle it */
	if (k_timer_status_get(&tc[port].timer_cc_debounce)) {
		k_timer_stop(&&tc[port].timer_cc_debounce);

		if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
			tc[port].typec_curr = usb_get_typec_current_limit(
				tc[port].polarity, cc1, cc2);

			typec_set_input_current_limit(port,
				tc[port].typec_curr, TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(port, CAP_DEDICATED);
		}
	}
}


/*
 * TYPE-C State Implementations
 */

/**
 * Disabled
 *
 * Super State Entry Actions:
 *   Remove the terminations from CC
 *   Set VBUS and VCONN off
 */
__overridable void tc_snk_disabled_entry(const int port)
{
	print_current_state(port);
	/*
	 * We have completed tc_cc_open_entry (our super state), so set flag
	 * to indicate to pd_is_port_enabled that we are now suspended.
	 */
	TC_SET_FLAG(port, TC_FLAGS_SUSPENDED);
}

__overridable void tc_snk_disabled_run(const int port)
{
	/* If pd_set_suspend clears the request, go to TC_UNATTACHED_SNK. */
	if (!TC_CHK_FLAG(port, TC_FLAGS_REQUEST_SUSPEND)) {
		set_state_tc(port, TC_UNATTACHED_SNK);
		return;
	}
	tc_pause_event_loop(port);
}

__overridable void tc_snk_disabled_exit(const int port)
{
	int rv;

	tc_start_event_loop(port);
	TC_CLR_FLAG(port, TC_FLAGS_SUSPENDED);

	rv = tcpm_init(port);
	LOG_PRINTK("C%d: TCPC init %s\n", port, rv ? "failed!" : "ready");
}

/**
 * ErrorRecovery
 *
 * Super State Entry Actions:
 *   Remove the terminations from CC
 *   Set's VBUS and VCONN off
 */
__overridable void tc_snk_error_recovery_entry(const int port)
{
	print_current_state(port);

	k_timer_start(&tc[port].timer_timeout,
		      K_MSEC(PD_T_ERROR_RECOVERY)
		      K_NO_WAIT);

	TC_CLR_FLAG(port, TC_FLAGS_REQUEST_ERROR_RECOVERY);
}

__overridable void tc_snk_error_recovery_run(const int port)
{
	if (!k_timer_status_get(&tc[port].timer_timeout))
		return;

	/*
	 * If we transitioned to error recovery as the first state and we
	 * didn't brown out, we don't need to reinitialized the tc statemachine
	 * because we just did that. So transition to the state directly.
	 */
	if (tc[port].ctx.previous == NULL) {
		set_state_tc(port, TC_UNATTACHED_SNK);
		return;
	}
	restart_tc_sm(port, TC_UNATTACHED_SNK);
}

__overridable void tc_snk_error_recovery_exit(const int port)
{
	k_timer_stop(&tc[port].timer_timeout);
}

/**
 * Unattached.SNK
 */
__overridable void tc_snk_unattached_snk_entry(const int port)
{
	enum pd_data_role prev_data_role;

	tc_detached(port);
	print_current_state(port);

	/*
	 * We are in an unattached state and considering to be a SNK
	 * searching for a SRC partner.  We set the CC pull value to
	 * to indicate our intent to be SNK in hopes a partner SRC
	 * will is there to attach to.
	 *
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rd.
	 *
	 * Restore default current limit Rp in case we swap to source
	 *
	 * Run any debug detaches needed before setting CC, as some TCPCs may
	 * require we set CC Open before changing power roles with a debug
	 * accessory.
	 */
	if (INCLUDE_USB_TC_DBGACC_OPTION)
		tcpm_debug_detach(port);

	typec_select_pull(port, TYPEC_CC_RD);
	typec_select_src_current_limit_rp(port,
		typec_get_default_current_limit_rp(port));
	typec_update_cc(port);


	prev_data_role = tc[port].data_role;
	tc[port].data_role = PD_ROLE_DISCONNECTED;
	/*
	 * When data role set events are used to enable BC1.2, then CC
	 * detach events are used to notify BC1.2 that it can be powered
	 * down.
	 */
	bc12_role_change_handler(port, prev_data_role, tc[port].data_role);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charge_manager_update_dualrole(port, CAP_UNKNOWN);

	tc_set_partner_role(port, PPC_DEV_DISCONNECTED);

	/*
	 * Indicate that the port is disconnected.
	 */
	pd_execute_data_swap(port, PD_ROLE_DISCONNECTED);
}

__overridable void tc_snk_unattached_snk_run(const int port)
{
	enum tcpc_cc_voltage_status cc1, cc2;

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
	if (cc_is_rp(cc1) || cc_is_rp(cc2)) {
		/* Connection Detected */
		set_state_tc(port, TC_ATTACH_WAIT_SNK);
		return;
	}

	/*
	 * Initialize type-C supplier current limits to 0. The charge
	 * manage is now seeded if it was not.
	 */
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		typec_set_input_current_limit(port, 0, 0);
}

__overridable void tc_snk_unattached_snk_exit(const int port)
{
}

/**
 * AttachWait.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
__overridable void tc_snk_attach_wait_snk_entry(const int port)
{
	print_current_state(port);

	tc[port].cc_state = PD_CC_UNSET;
}

__overridable void tc_snk_attach_wait_snk_run(const int port)
{
	enum tcpc_cc_voltage_status cc1, cc2;
	enum pd_cc_states new_cc_state;

	/* Check for connection */
	tcpm_get_cc(port, &cc1, &cc2);

	if (cc_is_rp(cc1) && cc_is_rp(cc2) && board_is_dts_port(port))
		new_cc_state = PD_CC_DFP_DEBUG_ACC;
	else if (cc_is_rp(cc1) || cc_is_rp(cc2))
		new_cc_state = PD_CC_DFP_ATTACHED;
	else
		new_cc_state = PD_CC_NONE;

	/* Debounce the cc state */
	if (new_cc_state != tc[port].cc_state) {
		k_timer_start(&tc[port].timer_cc_debounce,
			      K_MSEC(PD_T_CC_DEBOUNCE),
			      K_NO_WAIT);
		tc[port].cc_state = new_cc_state;
		return;
	}

	/* Wait for CC debounce */
	if (!k_timer_status_get(&tc[port].timer_cc_debounce))
		return;

	/* VBUS is not present, go back to Unattached.SNK */
	if (!pd_is_vbus_present(port))
		set_state_tc(port, TC_UNATTACHED_SNK);

	/*
	 * The port shall transition to Attached.SNK after the state of only
	 * one of the CC1 or CC2 pins is SNK.Rp for at least tCCDebounce and
	 * VBUS is detected.
	 *
	 * If the port supports Debug Accessory Mode, the port shall transition
	 * to DebugAccessory.SNK if the state of both the CC1 and CC2 pins is
	 * SNK.Rp for at least tCCDebounce and VBUS is detected.
	 */
	else if (INCLUDE_USB_TC_DBGACC_OPTION &&
		 tc[port].supports_debugacc)
		set_state_tc(port,
			     (new_cc_state == PD_CC_DFP_ATTACHED)
				? TC_ATTACHED_SNK
				: TC_DEBUG_ACCESSORY_SNK);
	else
		set_state_tc(port, TC_ATTACHED_SNK);
}

__overridable void tc_snk_attach_wait_snk_exit(const int port)
{
	k_timer_stop(&tc[port].timer_cc_debounce);
}

/**
 * Attached.SNK, shared with Debug Accessory.SNK
 */
__overridable void tc_snk_attached_debug_accessory_snk_entry(const int port)
{
	enum tcpc_cc_voltage_status cc1, cc2;

	/*
	 * Known state of attach is SNK.  We need to apply this pull value
	 * to make it set in hardware at the correct time but set the common
	 * pull here.
	 *
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rd.
	 */
	typec_select_pull(port, TYPEC_CC_RD);

	/* Inform the PPC and OCP module that a source is connected */
	tc_set_partner_role(port, PPC_DEV_SRC);

	/* Get connector orientation */
	tcpm_get_cc(port, &cc1, &cc2);
	tc[port].polarity = get_snk_polarity(cc1, cc2);
	pd_set_polarity(port, tc[port].polarity);

	tc_set_data_role(port, PD_ROLE_UFP);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		tc[port].typec_curr =
		usb_get_typec_current_limit(tc[port].polarity,
							cc1, cc2);
		typec_set_input_current_limit(port,
				tc[port].typec_curr, TYPE_C_VOLTAGE);
		/*
		 * Start new connections as dedicated until source caps
		 * are received, at which point the PE will update the
		 * flag.
		 */
		charge_manager_update_dualrole(port, CAP_DEDICATED);
	}

	/* Apply Rd */
	typec_update_cc(port);

	/*
	* Attached.SNK - enable AutoDischargeDisconnect
	* Do this after applying Rd to CC lines to avoid
	* TCPC_REG_FAULT_STATUS_AUTO_DISCHARGE_FAIL (b/171567398)
	*/
	tcpm_enable_auto_discharge_disconnect(port, 1);

	k_timer_stop(&tc[port].timer_cc_debounce);
}
__overridable void tc_snk_attached_snk_entry(const int port)
{
	print_current_state(port);

	tc_snk_attached_debug_accessory_snk_entry(port);
}
#if (INCLUDE_USB_TC_DBGACC_OPTION)
__overridable void tc_snk_debug_accessory_snk_entry(const int port)
{
	print_current_state(port);

	tc_snk_attached_debug_accessory_snk_entry(port);
	tcpm_debug_accessory(port, 1);
}
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */


__overridable void tc_snk_attached_debug_accessory_snk_run(const int port)
{
	/* Detach detection */
	if (pd_check_vbus_level(port, VBUS_REMOVED)) {
		set_state_tc(port, TC_UNATTACHED_SNK);
		return;
	}

	/* Run Sink Power Sub-State */
	sink_power_sub_states(port);
}
__overridable void tc_snk_attached_snk_run(const int port)
{
	tc_snk_attached_debug_accessory_snk_run(port);
}
#if (INCLUDE_USB_TC_DBGACC_OPTION)
__overridable void tc_snk_debug_accessory_snk_run(const int port)
{
	tc_snk_attached_debug_accessory_snk_run(port);
}
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */


__overridable void tc_snk_attached_debug_accessory_snk_exit(const int port)
{
	/*
	 * Attached.SNK exit - disable AutoDischargeDisconnect
	 * NOTE: This should not happen if we are suspending. It will
	 * happen in tc_cc_open_entry if that is the path we are
	 * taking.
	 */
	if (!TC_CHK_FLAG(port, TC_FLAGS_REQUEST_SUSPEND))
		tcpm_enable_auto_discharge_disconnect(port, 0);
	}

	/* Clear flags after checking Vconn status */
	TC_CLR_FLAG(port, TC_FLAGS_POWER_OFF_SNK);

	/* Stop drawing power */
	sink_stop_drawing_current(port);

	k_timer_stop(&tc[port].timer_cc_debounce);
	k_timer_stop(&tc[port].timer_timeout);
}
__overridable void tc_snk_attached_snk_exit(const int port)
{
	tc_snk_attached_debug_accessory_snk_exit(port);
}
#if (INCLUDE_USB_TC_DBGACC_OPTION)
__overridable void tc_snk_debug_accessory_snk_exit(const int port)
{
	tc_snk_attached_debug_accessory_snk_exit(port);

	tcpm_debug_detach(port);
}
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */


/**
 * Super State CC_RD
 */
__overridable void tc_snk_cc_rd_entry(const int port)
{
	tcpm_set_msg_header(port, PD_ROLE_SINK, tc[port].data_role);
}


/**
 * Super State CC_OPEN
 */
__overridable void tc_snk_cc_open_entry(const int port)
{
	/*
	 * Ensure we disable discharging before setting CC lines to open.
	 * If we were sourcing above, then we already drained Vbus. If partner
	 * is sourcing Vbus they will drain Vbus if they are PD-capable. This
	 * should only be done if a battery is present as a batteryless
	 * device will brown out when AutoDischargeDisconnect is disabled and
	 * we do not want this to happen until the set_cc open/open to make
	 * sure the TCPC has managed its internal states for disconnecting
	 * the only source of power it has.
	 */
	if (battery_is_present())
		tcpm_enable_auto_discharge_disconnect(port, 0);

	/*
	 * We may brown out after applying CC open, so flush console first.
	 * Console flush can take a long time, so if we aren't in danger of
	 * browning out, don't do it so we can meet certain compliance timing
	 * requirements.
	 */
	LOG_PRINTK("C%d: Applying CC Open!\n", port);
	if (!battery_is_present())
		cflush();

	/* Remove terminations from CC */
	typec_select_pull(port, TYPEC_CC_OPEN);
	typec_update_cc(port);

	tc_set_partner_role(port, PPC_DEV_DISCONNECTED);
	tc_detached(port);
}

void tc_set_debug_level(enum debug_level debug_level)
{
#ifndef CONFIG_USB_PD_DEBUG_LEVEL
	tc_debug_level = debug_level;
#endif
}

void tc_run(const int port)
{
	/*
	 * If pd_set_suspend set TC_FLAGS_REQUEST_SUSPEND, go directly to
	 * TC_DISABLED.
	 */
	if (get_state_tc(port) != TC_DISABLED
	    && TC_CHK_FLAG(port, TC_FLAGS_REQUEST_SUSPEND)) {
		set_state_tc(port, TC_DISABLED);
	}

	/* If error recovery has been requested, transition now */
	if (TC_CHK_FLAG(port, TC_FLAGS_REQUEST_ERROR_RECOVERY)) {
		set_state_tc(port, TC_ERROR_RECOVERY);
	}

	run_state(port, &tc[port].ctx);
}

static void pd_set_power_change(void)
{
	int i;

	for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; i++) {
		task_set_event(PD_PORT_TO_TASK_ID(i),
			       PD_EVENT_POWER_STATE_CHANGE);
	}
}
DECLARE_DEFERRED(pd_set_power_change);

/*
 * Type-C State Hierarchy (Sub-States are listed inside the boxes)
 *
 * |TC_CC_RD --------------|
 * |			   |
 * |	TC_UNATTACHED_SNK  |
 * |	TC_ATTACH_WAIT_SNK |
 * |	TC_TRY_WAIT_SNK    |
 * |-----------------------|
 *
 * |TC_CC_OPEN -----------|
 * |                      |
 * |	TC_DISABLED       |
 * |	TC_ERROR_RECOVERY |
 * |----------------------|
 *
 * TC_ATTACHED_SNK TC_DEBUG_ACCESSORY_SNK
 *
 */
static __const_data const struct usb_state tc_states[] = {
	/* Super States */
	[TC_CC_OPEN] = {
		.entry	= tc_snk_cc_open_entry,
	},
	[TC_CC_RD] = {
		.entry	= tc_snk_cc_rd_entry,
	},
	/* Normal States */
	[TC_DISABLED] = {
		.entry	= tc_snk_disabled_entry,
		.run	= tc_snk_disabled_run,
		.exit	= tc_snk_disabled_exit,
		.parent = &tc_states[TC_CC_OPEN],
	},
	[TC_ERROR_RECOVERY] = {
		.entry	= tc_snk_error_recovery_entry,
		.run	= tc_snk_error_recovery_run,
		.exit   = tc_snk_error_recovery_exit,
		.parent = &tc_states[TC_CC_OPEN],
	},
	[TC_UNATTACHED_SNK] = {
		.entry	= tc_snk_unattached_snk_entry,
		.run	= tc_snk_unattached_snk_run,
		.exit	= tc_snk_unattached_snk_exit,
		.parent = &tc_states[TC_CC_RD],
	},
	[TC_ATTACH_WAIT_SNK] = {
		.entry	= tc_snk_attach_wait_snk_entry,
		.run	= tc_snk_attach_wait_snk_run,
		.exit	= tc_snk_attach_wait_snk_exit,
		.parent = &tc_states[TC_CC_RD],
	},
	[TC_ATTACHED_SNK] = {
		.entry	= tc_snk_attached_snk_entry,
		.run	= tc_snk_attached_snk_run,
		.exit	= tc_snk_attached_snk_exit,
	},
#if (INCLUDE_USB_TC_DBGACC_OPTION)
	[TC_DEBUG_ACCESSORY_SNK] = {
		.entry	= tc_snk_debug_accessory_snk_entry,
		.run	= tc_snk_debug_accessory_snk_run,
		.exit	= tc_snk_debug_accessory_snk_exit,
	},
#endif /* INCLUDE_USB_TC_DBGACC_OPTION */
};

#if defined(TEST_BUILD) && defined(INCLUDE_USB_TC_DEBUG_LABELS)
const struct test_sm_data test_tc_sm_data[] = {
	{
		.base = tc_states,
		.size = ARRAY_SIZE(tc_states),
		.names = tc_state_names,
		.names_size = ARRAY_SIZE(tc_state_names),
	},
};
const int test_tc_sm_data_size = ARRAY_SIZE(test_tc_sm_data);
#endif
