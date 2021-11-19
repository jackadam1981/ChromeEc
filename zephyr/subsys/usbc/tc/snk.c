/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * USBC TypeC SNK with DebugAccessory State Machine
 *   See Figure 4-13 in Release 2.1 of USB Type-C Spec.
 */
#include <logging/log.h>
#include "usbc_tc_sm.h"
#include "usbc_tcpc.h"

LOG_MODULE_REGISTER(usb_tc_sm, CONFIG_USB_TC_SM_LOG_LEVEL);


/*****************************************************************************
 * Forward Declarations
 */
/* Per Port TypeC information */
/*
 * TODO: tc needs to be filled in with DeviceTree information and should be
 * done at compiled time.
 */
static struct usb_tc_port_info tc[CONFIG_USB_PD_PORT_MAX_COUNT];

/* Full list of TypeC states. This is indexed by usb_tc_state */
static const struct usb_state tc_states[];


/* Private functions */
static void set_state_tc(
		const struct usb_tc_port_info *tc_port,
		const enum usb_tc_state new_state);
static enum usb_tc_state get_state_tc(
		const struct usb_tc_port_info *tc_port);


/*****************************************************************************
 * Debug
 */
/* List of human readable state names for console debugging */
static __unused __const_data const char * const tc_state_names[] = {
#if (CONFIG_USB_TC_SM_LOG_LEVEL != LOG_LEVEL_NONE)
	[TC_DISABLED] = "Disabled",
	[TC_ERROR_RECOVERY] = "ErrorRecovery",
	[TC_UNATTACHED_SNK] = "Unattached.SNK",
	[TC_ATTACH_WAIT_SNK] = "AttachWait.SNK",
	[TC_ATTACHED_SNK] = "Attached.SNK",
#if (USB_TC_INCLUDES_DBGACC_OPTION)
	[TC_DEBUG_ACCESSORY_SNK] = "DebugAccessory.SNK"
#endif /* USB_TC_INCLUDES_DBGACC_OPTION */

	/* Super States */
	[TC_CC_OPEN] = "SS:CC_OPEN",

	[TC_STATE_COUNT] = "",
#endif
};

/* Debug log level - higher number == more log */
#ifdef CONFIG_USB_PD_DEBUG_LEVEL
static const enum debug_level tc_debug_level = CONFIG_USB_PD_DEBUG_LEVEL;
#else
static enum debug_level tc_debug_level = DEBUG_LEVEL_1;
#endif

void tc_set_debug_level(enum debug_level debug_level)
{
	if (!IS_ENABLED(CONFIG_USB_PD_DEBUG_LEVEL))
		tc_debug_level = debug_level;
}

/*****************************************************************************
 * Overridable Functions
 */

/*
 * Default usb_tc_sm_initial_state will be TC_ERROR_RECOVER
 *
 * The equivalent for CrOS would be something like the following as an
 * override
 *
 *	__override enum usb_tc_state usb_tc_sm_initial_state(
 *		struct usb_tc_port_info *tc_port)
 *	{
 *		enum usb_tc_state first_state;
 *
 *		// We are going to apply CC open (start with ErrorRecovery
 *		// state) unless there is something which forbids us to do
 *		// that (one of conditions below is true)
 *		first_state = TC_ERROR_RECOVERY;
 *
 *		// If we just lost power, don't apply CC open. Otherwise we
 *		// would boot loop, and if this is a fresh power on, then we
 *		// know there isn't any stale PD state as well.
 *		if (system_get_reset_flags() &
 *		    (EC_RESET_FLAG_BROWNOUT | EC_RESET_FLAG_POWER_ON))
 *			first_state = TC_UNATTACHED_SNK;
 *
 *		// If this is non-EFS2 device, battery is not present and
 *		// EC RO doesn't keep power-on reset flag after reset caused
 *		// by H1, then don't apply CC open because it will cause
 *		// brown out.
 *		//
 *		// Please note that we are checking if
 *		// CONFIG_BOARD_RESET_AFTER_POWER_ON is defined now, but
 *		// actually we need to know if it was enabled in EC RO!
 *		// It was assumed that if CONFIG_BOARD_RESET_AFTER_POWER_ON
 *		// is defined now it was defined in EC RO too.
 *		if (!IS_ENABLED(CONFIG_BOARD_RESET_AFTER_POWER_ON) &&
 *		    !IS_ENABLED(CONFIG_VBOOT_EFS2) &&
 *		    IS_ENABLED(CONFIG_BATTERY) &&
 *		    (battery_is_present() == BP_NO))
 *			first_state = TC_UNATTACHED_SNK;
 *
 *		return first_state;
 *	}
 */
__overridable enum usb_tc_state usb_tc_sm_initial_state(
	struct usb_tc_port_info *tc_port)
{
	return TC_ERROR_RECOVERY;
}

/*****************************************************************************
 * Public Functions
 */
const char *tc_get_current_state(struct usb_tc_port_info *tc_port)
{
	if (CONFIG_USB_TC_SM_LOG_LEVEL != LOG_LEVEL_NONE)
		return tc_state_names[get_state_tc(tc_port)];
	else
		return "";
}

uint32_t tc_get_flags(struct usb_tc_port_info *tc_port)
{
	return tc_port->flags;
}

bool tc_is_attached_snk(struct usb_tc_port_info *tc_port)
{
	return IS_ATTACHED_SNK(tc_port);
}

static void tc_detach(struct usb_tc_port_info *tc_port)
{
	int rv;

	if (USB_TC_INCLUDES_DBGACC_OPTION &&
	    tc_port->supports_debug_accessory) {
		rv = tcpc_set_debug_accessory(tc_port->tcpc, false);
		if (rv) {
			LOG_ERR("C%d: TCPC set DebugAcc failed (%d)",
				tc_port->port, rv);
		}
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

void pd_set_suspend(struct usb_tc_port_info *tc_port, int suspend)
{
	if (pd_is_port_enabled(tc_port) == !suspend)
		return;

	/* Track if we are suspended or not */
	if (suspend) {
		int wait = 0;

		TC_SET_FLAG(tc_port, TC_FLAGS_REQUEST_SUSPEND);

		/*
		 * Avoid deadlock when running from task
		 * which we are going to suspend
		 */
		if (tc_port->tid == k_current_get())
			return;

		k_wakeup(tc_port->tid);

		/* Sleep this task if we are not suspended */
		while (pd_is_port_enabled(tc_port)) {
			if (++wait > SUSPEND_SLEEP_RETRIES) {
				LOG_WARN("C%d: NOT SUSPENDED after %dms\n",
					tc_port->port,
					wait * SUSPEND_SLEEP_DELAY);
				return;
			}
			msleep(SUSPEND_SLEEP_DELAY);
		}
	} else {
		TC_CLR_FLAG(tc_port, TC_FLAGS_REQUEST_SUSPEND);
		k_wakeup(tc_port->tid);
	}
}

void pd_set_error_recovery(struct usb_tc_port_info *tc_port)
{
	TC_SET_FLAG(tc_port, TC_FLAGS_REQUEST_ERROR_RECOVERY);
}

bool pd_is_port_enabled(struct usb_tc_port_info *tc_port)
{
	/*
	 * Checking get_state_tc(port) from another task isn't safe since it
	 * can return TC_DISABLED before tc_cc_open_entry and tc_disabled_entry
	 * are complete. So check TC_FLAGS_SUSPENDED instead.
	 */
	return !TC_CHK_FLAG(tc_port, TC_FLAGS_SUSPENDED);
}

/*
 * TCPC CC/Rp management
 */
static void tc_select_pull(struct usb_tc_port_info *tc_port,
		enum tcpc_cc_pull pull)
{
	tc_port->select_cc_pull = pull;
}

int tc_update_cc(struct usb_tc_port_info *tc_port)
{
	int rv;
	enum tc_cc_pull pull = tc_port->select_cc_pull;
	enum tc_rp_value rp = TC_RP_USB;

	rv = tcpc_set_cc(tc_port->tcpc, pull, rp);
	if (rv)
		LOG_ERR("C%d: TCPC set CC failed (%d)", tc_port->port, rv);
	return rv;
}

void tc_start_error_recovery(struct usb_tc_port_info *tc_port)
{
	assert(tc_port->tid == k_current_get());

	/*
	 *   The port should transition to the ErrorRecovery state
	 *   from any other state when directed.
	 */
	set_state_tc(tc_port, TC_ERROR_RECOVERY);
}

static void restart_tc_sm(struct usb_tc_port_info *tc_port,
		enum usb_tc_state start_state)
{
	int rv;

	/* Clear flags before we transitions states */
	tc_port->flags = 0;

	rv = tcpc_init(tc_port->tcpc);

	LOG_PRINTK("C%d: TCPC init %s\n", tc_port->port,
		   rv ? "failed" : "ready");

	/* Disable if restart failed, otherwise start in default state. */
	set_state_tc(tc_port, rv ? TC_DISABLED : start_state);

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charge_manager_update_dualrole(tc_port->port, CAP_UNKNOWN);
}

void tc_state_init(struct usb_tc_port_info *tc_port)
{
	enum usb_tc_state first_state;

	/* Store the Thread ID */
	tc_port->tid = k_current_get();

	/* For test builds, replicate static initialization */
	if (IS_ENABLED(TEST_BUILD)) {
		int i;

		for (i = 0; i < CONFIG_USB_PD_PORT_MAX_COUNT; ++i)
			memset(&tc[i], 0, sizeof(tc[i]));
	}

	/*
	 * Determine the initial state to start this port's state machine.
	 */
	first_state = usb_tc_sm_initial_state(port);

	/*
	 * Initilialize the timers used in this TC state machine
	 *	CC_DEBOUNCE, PD_DEBOUNCE and TIMEOUT
	 */
	k_timer_init(&tc_port->timer_cc_debounce, NULL, NULL);
	k_timer_init(&tc_port->timer_pd_debounce, NULL, NULL);
	k_timer_init(&tc_port->timer_timeout, NULL, NULL);

#ifdef CONFIG_USB_PD_TCPC_BOARD_INIT
	/* Board specific TCPC init */
	board_tcpc_init();
#endif

	/*
	 * Start the state machine.
	 */
	restart_tc_sm(tc_port, first_state);
}

uint8_t tc_get_polarity(struct usb_tc_port_info *tc_port)
{
	return tc_port->polarity;
}

/*
 * Private Functions
 */

/* Set the TypeC state machine to a new state. */
static void set_state_tc(const struct usb_tc_port_info *tc_port,
		const enum usb_tc_state new_state)
{
	assert(tc_port->tid == k_current_get());

	set_state(tc_port, &tc_port->ctx, &tc_states[new_state]);
}

/* Get the current TypeC state. */
static enum usb_tc_state get_state_tc(const struct usb_tc_port_info *tc_port)
{
	/* Default to returning TC_STATE_COUNT if no state has been set */
	if (tc_port->ctx.current == NULL)
		return TC_STATE_COUNT;
	else
		return tc_port->ctx.current - &tc_states[0];
}

/* Get the previous TypeC state. */
static enum usb_tc_state get_last_state_tc(
				const struct usb_tc_port_info *tc_port)
{
	return tc_port->ctx.previous - &tc_states[0];
}

static void print_current_state(const struct usb_tc_port_info *tc_port)
{
	if (CONFIG_USB_TC_SM_LOG_LEVEL != LOG_LEVEL_NONE) {
		if (tc_debug_level > 1) {
			LOG_INF("C%d: %s\n", tc_port->port,
				tc_state_names[get_state_tc(tc_port)]);
		}
	} else {
		LOG_PRINTK("C%d: tc-st%d\n", tc_port->port,
			   get_state_tc(tc_port));
	}
}


static void sink_stop_drawing_current(struct usb_tc_port_info *tc_port)
{
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		tc_set_input_current_limit(tc_port->port, 0, 0);
		charge_manager_set_ceil(tc_port->port,
				CEIL_REQUESTOR_PD, CHARGE_CEIL_NONE);
	}
}

static void sink_power_sub_states(struct usb_tc_port_info *tc_port)
{
	int rv;
	enum tc_cc_voltage_status cc1, cc2, cc;
	enum tc_cc_voltage_status new_cc_voltage;

	rv = tcpc_get_cc(tc_port->tcpc, &cc1, &cc2);
	if (rv) {
		LOG_ERR("C%d: TCPC get CC failed (%d)", tc_port->port, rv);
		return;
	}

	cc = polarity_rm_dts(tc_port->polarity) ? cc2 : cc1;

	switch (cc)
	case TC_CC_VOLT_RP_DEF:
	case TC_CC_VOLT_RP_1_5:
	case TC_CC_VOLT_RP_3_0:
		new_cc_voltage = cc;
		break;
	default:
		new_cc_voltage = TC_CC_VOLT_OPEN;
	}

	/* Debounce the cc state */
	if (new_cc_voltage != tc_port->cc_voltage) {
		tc_port->cc_voltage = new_cc_voltage;
		k_timer_start(&tc_port->timer_cc_debounce,
			      K_MSEC(PD_T_RP_VALUE_CHANGE),
			      K_NO_WAIT);
		return;
	}

	/* If the timer has expired, handle it */
	if (k_timer_status_get(&tc_port->timer_cc_debounce)) {
		k_timer_stop(&tc_port->timer_cc_debounce);

		if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
			tc_port->typec_curr = usb_get_typec_current_limit(
				tc_port->polarity, cc1, cc2);

			tc_set_input_current_limit(tc_port->port,
				tc_port->typec_curr, TYPE_C_VOLTAGE);
			charge_manager_update_dualrole(tc_port->port,
				CAP_DEDICATED);
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
static void tc_snk_disabled_entry(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);
	/*
	 * We have completed tc_cc_open_entry (our super state), so set flag
	 * to indicate to pd_is_port_enabled that we are now suspended.
	 */
	TC_SET_FLAG(tc_port, TC_FLAGS_SUSPENDED);
}

static void tc_snk_disabled_run(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/* If pd_set_suspend clears the request, go to TC_UNATTACHED_SNK. */
	if (!TC_CHK_FLAG(tc_port, TC_FLAGS_REQUEST_SUSPEND)) {
		set_state_tc(tc_port, TC_UNATTACHED_SNK);
		return;
	}
	tc_pause_event_loop(tc_port);
}

static void tc_snk_disabled_exit(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	tc_start_event_loop(tc_port);
	TC_CLR_FLAG(tc_port, TC_FLAGS_SUSPENDED);

	rv = tcpc_init(tc_port->tcpc);
	LOG_PRINTK("C%d: TCPC init %s\n", tc_port->port,
		   rv ? "failed!" : "ready");
}

/**
 * ErrorRecovery
 *
 * Super State Entry Actions:
 *   Remove the terminations from CC
 *   Set's VBUS and VCONN off
 */
static void tc_snk_error_recovery_entry(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);

	k_timer_start(&tc_port->timer_timeout,
		      K_MSEC(PD_T_ERROR_RECOVERY)
		      K_NO_WAIT);

	TC_CLR_FLAG(tc_port, TC_FLAGS_REQUEST_ERROR_RECOVERY);
}

static void tc_snk_error_recovery_run(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	if (!k_timer_status_get(&tc_port->timer_timeout))
		return;

	/*
	 * If we transitioned to error recovery as the first state and we
	 * didn't brown out, we don't need to reinitialized the tc statemachine
	 * because we just did that. So transition to the state directly.
	 */
	if (tc_port->ctx.previous == NULL) {
		set_state_tc(tc_port, TC_UNATTACHED_SNK);
		return;
	}
	restart_tc_sm(tc_port, TC_UNATTACHED_SNK);
}

static void tc_snk_error_recovery_exit(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	k_timer_stop(&tc_port->timer_timeout);
}

/**
 * Unattached.SNK
 */
static void tc_snk_unattached_snk_entry(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);

	/* Detach from the TC Port */
	tc_detach(tc_port);

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
	if (USB_TC_INCLUDES_DBGACC_OPTION &&
	    tc_port->supports_debug_accessory) {
		rv = tcpc_set_debug_detach(tc_port->tcpc);
		if (rv) {
			LOG_ERR("C%d: TCPC set Debug Detach failed (%d)",
				tc_port->port, rv);
			return;
		}
	}

	tc_select_pull(tc_port, TC_CC_RD);
	tc_update_cc(tc_port);

	/* Notify TCPC of role update */
	rv = tcpc_set_roles(tc_port->tcpc,
			    TC_ROLE_SINK,
			    TC_ROLE_DISCONNECTED);
	if (rv) {
		LOG_ERR("C%d: TCPC set role failed (%d)", tc_port->port, rv);
		return;
	}

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		charge_manager_update_dualrole(tc_port->port, CAP_UNKNOWN);
}

static void tc_snk_unattached_snk_run(void *tc_port_obj)
{
	int rv;
	enum tc_cc_voltage_state cc1, cc2;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/* Check for connection */
	rv = tcpc_get_cc(tc_port->tcpc, &cc1, &cc2);
	if (ret) {
		LOG_ERR("C%d: TCPC get CC failed (%d)", tc_port->port, rv);
		return;
	}

	/*
	 * The port shall transition to AttachWait.SNK when a Source
	 * connection is detected, as indicated by the SNK.Rp state
	 * on at least one of its CC pins.
	 */
	if (cc_is_rp(cc1) || cc_is_rp(cc2)) {
		/* Connection Detected */
		set_state_tc(tc_port, TC_ATTACH_WAIT_SNK);
		return;
	}

	/*
	 * Initialize type-C supplier current limits to 0. The charge
	 * manage is now seeded if it was not.
	 */
	if (IS_ENABLED(CONFIG_CHARGE_MANAGER))
		tc_set_input_current_limit(tc_port, 0, 0);
}

/**
 * AttachWait.SNK
 *
 * Super State Entry Actions:
 *   Vconn Off
 *   Place Rd on CC
 *   Set power role to SINK
 */
static void tc_snk_attach_wait_snk_entry(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);

	tc_port->cc_state = PD_CC_UNSET;
}

static void tc_snk_attach_wait_snk_run(void *tc_port_obj)
{
	int rv;
	enum tc_cc_voltage_status cc1, cc2;
	enum tc_cc_states new_cc_state;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/* Check for connection */
	rv = tcpc_get_cc(tc_port->tcpc, &cc1, &cc2);
	if (rv) {
		LOG_ERR("C%d: TCPC get CC failed (%d)", tc_port->port, rv);
		return;
	}

	if (cc_is_rp(cc1) && cc_is_rp(cc2) && board_is_dts_port(tc_port))
		new_cc_state = PD_CC_DFP_DEBUG_ACC;
	else if (cc_is_rp(cc1) || cc_is_rp(cc2))
		new_cc_state = PD_CC_DFP_ATTACHED;
	else
		new_cc_state = PD_CC_NONE;

	/* Debounce the cc state */
	if (new_cc_state != tc_port->cc_state) {
		k_timer_start(&tc_port->timer_cc_debounce,
			      K_MSEC(PD_T_CC_DEBOUNCE),
			      K_NO_WAIT);
		k_timer_start(&tc_port->timer_pd_debounce,
			      K_MSEC(PD_T_PD_DEBOUNCE),
			      K_NO_WAIT);
		tc_port->cc_state = new_cc_state;
		return;
	}

	/*
	 * Transition to Unattached.SNK when the state of both the CC1 and
	 * CC2 pins is SNK.Open for at least tPDDebounce.
	 */
	if (new_cc_state == PD_CC_NONE &&
	    k_timer_status_get(&tc_port->timer_pd_debounce)) {
		/* We are detached */
		set_state_tc(tc_port, TC_UNATTACHED_SNK);
		return;
	}

	/* Wait for CC debounce */
	if (!k_timer_status_get(&tc_port->timer_cc_debounce))
		return;

	/*
	 * The port shall transition to Attached.SNK after the state of only
	 * one of the CC1 or CC2 pins is SNK.Rp for at least tCCDebounce and
	 * VBUS is detected.
	 *
	 * If the port supports Debug Accessory Mode, the port shall transition
	 * to DebugAccessory.SNK if the state of both the CC1 and CC2 pins is
	 * SNK.Rp for at least tCCDebounce and VBUS is detected.
	 */
	if (pd_is_vbus_present(tc_port)) {
		if (USB_TC_INCLUDES_DBGACC_OPTION &&
		    tc_port->supports_debug_accessory)
			set_state_tc(tc_port,
				(new_cc_state == PD_CC_DFP_ATTACHED)
					? TC_ATTACHED_SNK
					: TC_DEBUG_ACCESSORY_SNK);
		else
			set_state_tc(tc_port, TC_ATTACHED_SNK);
		return;
	}
}

static void tc_snk_attach_wait_snk_exit(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	k_timer_stop(&tc_port->timer_cc_debounce);
	k_timer_stop(&tc_port->timer_pd_debounce);
}

/**
 * Attached.SNK, shared with Debug Accessory.SNK
 */
static void tc_snk_attached_debug_accessory_snk_shared_entry(void *tc_port_obj)
{
	int rv;
	enum tc_cc_voltage_state cc1, cc2;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/*
	 * Known state of attach is SNK.  We need to apply this pull value
	 * to make it set in hardware at the correct time but set the common
	 * pull here.
	 *
	 * Both CC1 and CC2 pins shall be independently terminated to
	 * ground through Rd.
	 */
	tc_select_pull(tc_port, TC_CC_RD);

	/* Get connector orientation */
	rv = tcpc_get_cc(tc_port->tcpc, &cc1, &cc2);
	if (rv) {
		LOG_ERR("C%d: TCPC get CC failed (%d)", tc_port->port, rv);
		return;
	}

	tc_port->polarity = get_snk_polarity(cc1, cc2);
	pd_set_polarity(tc_port, tc_port->polarity);

	/* Notify TCPC of role update */
	rv = tcpc_set_roles(tc_port->tcpc,
			    TC_ROLE_SINK,
			    TC_ROLE_UFP);
	if (rv) {
		LOG_ERR("C%d: TCPC set role failed (%d)", tc_port->port, rv);
		return;
	}

	if (IS_ENABLED(CONFIG_CHARGE_MANAGER)) {
		tc_port->typec_curr =
		usb_get_typec_current_limit(tc_port->polarity,
				cc1, cc2);
		tc_set_input_current_limit(tc_port,
				tc_port->typec_curr, TYPE_C_VOLTAGE);

		/*
		 * Start new connections as dedicated until source caps
		 * are received, at which point the PE will update the
		 * flag.
		 */
		charge_manager_update_dualrole(tc_port, CAP_DEDICATED);
	}

	/* Apply Rd */
	tc_update_cc(tc_port);

	/*
	 * Attached.SNK - enable AutoDischargeDisconnect
	 * Do this after applying Rd to CC lines to avoid
	 * TCPC_REG_FAULT_STATUS_AUTO_DISCHARGE_FAIL
	 */
	rv = tcpc_enable_auto_discharge_disconnect(tc_port->tcpc, true);
	if (rv) {
		LOG_ERR("C%d: TCPC "
			"enable AutoDischargeDisconnect failed (%d)",
			tc_port->port, rv);
		return;
	}

	k_timer_stop(&tc_port->timer_cc_debounce);
}
static void tc_snk_attached_snk_entry(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);

	tc_snk_attached_debug_accessory_snk_shared_entry(tc_port);
}
#if (USB_TC_INCLUDES_DBGACC_OPTION)
static void tc_snk_debug_accessory_snk_entry(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	print_current_state(tc_port);

	tc_snk_attached_debug_accessory_snk_shared_entry(tc_port);

	rv = tcpc_set_debug_accessory(tc_port->tcpc, true);
	if (rv) {
		LOG_ERR("C%d: TCPC set DebugAcc failed (%d)",
			tc_port->port, rv);
		return;
	}
}
#endif /* USB_TC_INCLUDES_DBGACC_OPTION */


static void tc_snk_attached_debug_accessory_snk_shared_run(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/* Detach detection */
	if (pd_check_vbus_level(tc_port->port, VBUS_REMOVED)) {
		set_state_tc(tc_port, TC_UNATTACHED_SNK);
		return;
	}

	/* Run Sink Power Sub-State */
	sink_power_sub_states(tc_port);
}
static void tc_snk_attached_snk_run(void *tc_port_obj)
{
	tc_snk_attached_debug_accessory_snk_shared_run(tc_port_obj);
}
#if (USB_TC_INCLUDES_DBGACC_OPTION)
static void tc_snk_debug_accessory_snk_run(void *tc_port_obj)
{
	tc_snk_attached_debug_accessory_snk_shared_run(tc_port_obj);
}
#endif /* USB_TC_INCLUDES_DBGACC_OPTION */


static void tc_snk_attached_debug_accessory_snk_shared_exit(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/*
	 * Attached.SNK exit - disable AutoDischargeDisconnect
	 * NOTE: This should not happen if we are suspending. It will
	 * happen in tc_cc_open_entry if that is the path we are
	 * taking.
	 */
	if (!TC_CHK_FLAG(tc_port, TC_FLAGS_REQUEST_SUSPEND)) {
		rv = tcpc_enable_auto_discharge_disconnect(tc_port->tcpc,
							   false);
		if (rv) {
			LOG_ERR("C%d: TCPC "
				"enable AutoDischargeDisconnect failed (%d)",
				tc_port->port, rv);
			return;
		}
	}

	/* Stop drawing power */
	sink_stop_drawing_current(tc_port);

	k_timer_stop(&tc_port->timer_cc_debounce);
	k_timer_stop(&tc_port->timer_timeout);
}
static void tc_snk_attached_snk_exit(void *tc_port_obj)
{
	tc_snk_attached_debug_accessory_snk_shared_exit(tc_port_obj);
}
#if (USB_TC_INCLUDES_DBGACC_OPTION)
static void tc_snk_debug_accessory_snk_exit(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

	tc_snk_attached_debug_accessory_snk_shared_exit(tc_port);

	rv = tcpc_set_debug_detach(tc_port->tcpc);
	if (rv) {
		LOG_ERR("C%d: TCPC set Debug Detach failed (%d)",
			tc_port->port, rv);
		return;
	}
}
#endif /* USB_TC_INCLUDES_DBGACC_OPTION */


/**
 * Super State CC_OPEN
 */
static void tc_snk_cc_open_entry(void *tc_port_obj)
{
	int rv;
	struct usb_tc_port_info *tc_port = tc_port_obj;

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
	if (battery_is_present()) {
		rv = tcpc_enable_auto_discharge_disconnect(tc_port->tcpc,
							   false);
		if (rv) {
			LOG_ERR("C%d: TCPC "
				"enable AutoDischargeDisconnect failed (%d)",
				tc_port->port, rv);
			return;
		}
	}

	/*
	 * We may brown out after applying CC open, so flush console first.
	 * Console flush can take a long time, so if we aren't in danger of
	 * browning out, don't do it so we can meet certain compliance timing
	 * requirements.
	 */
	LOG_PRINTK("C%d: Applying CC Open!\n", tc_port->port);
	if (!battery_is_present())
		cflush();

	/* Remove terminations from CC */
	tc_select_pull(tc_port, TC_CC_OPEN);
	tc_update_cc(tc_port);

	/* Detach from the TC Port */
	tc_detach(tc_port);
}

void tc_run(void *tc_port_obj)
{
	struct usb_tc_port_info *tc_port = tc_port_obj;

	/*
	 * If pd_set_suspend set TC_FLAGS_REQUEST_SUSPEND, go directly to
	 * TC_DISABLED.
	 */
	if (get_state_tc(tc_port) != TC_DISABLED
	    && TC_CHK_FLAG(tc_port, TC_FLAGS_REQUEST_SUSPEND))
		set_state_tc(tc_port, TC_DISABLED);

	/* If error recovery has been requested, transition now */
	if (TC_CHK_FLAG(tc_port, TC_FLAGS_REQUEST_ERROR_RECOVERY))
		set_state_tc(tc_port, TC_ERROR_RECOVERY);

	run_state(tc_port, &tc_port->ctx);
}

/*
 * Type-C State Hierarchy (Sub-States are listed inside the boxes)
 *
 * |TC_CC_OPEN -----------|
 * |                      |
 * |	TC_DISABLED       |
 * |	TC_ERROR_RECOVERY |
 * |----------------------|
 *
 * TC_UNATTACHED_SNK   TC_ATTACH_WAIT_SNK   TC_ATTACHED_SNK
 * TC_DEBUG_ACCESSORY_SNK
 *
 */
static __const_data const struct usb_state tc_states[] = {
	/* Super States */
	[TC_CC_OPEN] = {
		.entry	= tc_snk_cc_open_entry,
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
	},
	[TC_ATTACH_WAIT_SNK] = {
		.entry	= tc_snk_attach_wait_snk_entry,
		.run	= tc_snk_attach_wait_snk_run,
		.exit	= tc_snk_attach_wait_snk_exit,
	},
	[TC_ATTACHED_SNK] = {
		.entry	= tc_snk_attached_snk_entry,
		.run	= tc_snk_attached_snk_run,
		.exit	= tc_snk_attached_snk_exit,
	},
#if (USB_TC_INCLUDES_DBGACC_OPTION)
	[TC_DEBUG_ACCESSORY_SNK] = {
		.entry	= tc_snk_debug_accessory_snk_entry,
		.run	= tc_snk_debug_accessory_snk_run,
		.exit	= tc_snk_debug_accessory_snk_exit,
	},
#endif /* USB_TC_INCLUDES_DBGACC_OPTION */
};
