/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <ap_power/ap_power_interface.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

#if CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI

/* If host doesn't program S0ix lazy wake mask, use default S0ix mask */
#define DEFAULT_WAKE_MASK_S0IX  (EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN) | \
				EC_HOST_EVENT_MASK(EC_HOST_EVENT_MODE_CHANGE))

/*
 * Set the wake mask according to the current power state:
 * 1. On transition to S0, wake mask is reset.
 * 2. In non-S0 states, active mask set by host gets a higher preference.
 * 3. If host has not set any active mask, then check if a lazy mask exists
 *    for the current power state.
 * 4. If state is S0ix and no lazy or active wake mask is set, then use default
 *    S0ix mask to be compatible with older BIOS versions.
 */
void power_update_wake_mask(void)
{
	host_event_t wake_mask;
	enum power_states_ndsx state;

	state = pwr_sm_get_state();

	if (state == SYS_POWER_STATE_S0)
		wake_mask = 0;
	else if (lpc_is_active_wm_set_by_host() ||
		ap_power_get_lazy_wake_mask(state, &wake_mask))
		return;
#if CONFIG_AP_PWRSEQ_S0IX
	if ((state == SYS_POWER_STATE_S0ix) && (wake_mask == 0))
		wake_mask = DEFAULT_WAKE_MASK_S0IX;
#endif

	lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, wake_mask);
}

static void power_update_wake_mask_deferred(struct k_work *work)
{
	power_update_wake_mask();
}

static K_WORK_DELAYABLE_DEFINE(
	power_update_wake_mask_deferred_data, power_update_wake_mask_deferred);

void ap_power_set_active_wake_mask(void)
{
	int rv;

	/*
	 * Allow state machine to stabilize and update wake mask after 5msec. It
	 * was observed that on platforms where host wakes up periodically from
	 * S0ix for hardware book-keeping activities, there is a small window
	 * where host is not really up and running software, but still SLP_S0#
	 * is de-asserted and hence setting wake mask right away can cause user
	 * wake events to be missed.
	 *
	 * Time for deferred callback was chosen to be 5msec based on the fact
	 * that it takes ~2msec for the periodic wake cycle to complete on the
	 * host for KBL.
	 */
	rv = k_work_schedule(&power_update_wake_mask_deferred_data, K_MSEC(5));
	if (rv == 0) {
		/*
		 * A work is already scheduled or submitted, since power state
		 * has changed again and the work is not processed, we should
		 * reschedule it.
		 */
		rv = k_work_reschedule(
			&power_update_wake_mask_deferred_data, K_MSEC(5));
	}
	__ASSERT(rv >= 0, "Set wake mask work queue error");
}

#else /* CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI */
static void ap_power_set_active_wake_mask(void) { }
#endif /* CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI */

#if CONFIG_AP_PWRSEQ_S0IX
/*
 * Backup copies of SCI and SMI mask to preserve across S0ix suspend/resume
 * cycle. If the host uses S0ix, BIOS is not involved during suspend and resume
 * operations and hence SCI/SMI masks are programmed only once during boot-up.
 *
 * These backup variables are set whenever host expresses its interest to
 * enter S0ix and then lpc_host_event_mask for SCI and SMI are cleared. When
 * host resumes from S0ix, masks from backup variables are copied over to
 * lpc_host_event_mask for SCI and SMI.
 */
static host_event_t backup_sci_mask;
static host_event_t backup_smi_mask;

/* Flag to notify listeners about suspend/resume events. */
enum ap_power_sleep_type sleep_state = AP_POWER_SLEEP_NONE;

/*
 * Clear host event masks for SMI and SCI when host is entering S0ix. This is
 * done to prevent any SCI/SMI interrupts when the host is in suspend. Since
 * BIOS is not involved in the suspend path, EC needs to take care of clearing
 * these masks.
 */
static void power_s0ix_suspend_clear_masks(void)
{
	backup_sci_mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SCI);
	backup_smi_mask = lpc_get_host_event_mask(LPC_HOST_EVENT_SMI);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, 0);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, 0);
}

/*
 * Restore host event masks for SMI and SCI when host exits S0ix. This is done
 * because BIOS is not involved in the resume path and so EC needs to restore
 * the masks from backup variables.
 */
static void power_s0ix_resume_restore_masks(void)
{
	/*
	 * No need to restore SCI/SMI masks if both backup_sci_mask and
	 * backup_smi_mask are zero. This indicates that there was a failure to
	 * enter S0ix(SLP_S0# assertion) and hence SCI/SMI masks were never
	 * backed up.
	 */
	if (!backup_sci_mask && !backup_smi_mask)
		return;
	lpc_set_host_event_mask(LPC_HOST_EVENT_SCI, backup_sci_mask);
	lpc_set_host_event_mask(LPC_HOST_EVENT_SMI, backup_smi_mask);
	backup_sci_mask = backup_smi_mask = 0;
}

/*
 * Following functions are called in the S0ix path, not S3 path.
 */

/*
 * Notify the sleep type that is going to transit to; this is a token to
 * ensure both host sleep event passed by Host Command and SLP_S0 satisfy
 * the conditions to suspend or resume.
 *
 * @param new_state Notified sleep type
 */
static void ap_power_sleep_set_notify(enum ap_power_sleep_type new_state)
{
	sleep_state = new_state;
}

enum ap_power_sleep_type ap_power_sleep_get_notify(void)
{
	return sleep_state;
}

void ap_power_sleep_notify_transition(enum ap_power_sleep_type check_state)
{
	if (sleep_state != check_state)
		return;

	if (check_state == AP_POWER_SLEEP_SUSPEND) {
		/*
		 * Transition to S0ix;
		 * clear mask before others running suspend.
		 */
		power_s0ix_suspend_clear_masks();
		ap_power_ev_send_callbacks(AP_POWER_SUSPEND);
	} else if (check_state == AP_POWER_SLEEP_RESUME) {
		ap_power_ev_send_callbacks(AP_POWER_RESUME);
	}

	/* Transition is done; reset sleep state. */
	ap_power_sleep_set_notify(AP_POWER_SLEEP_NONE);
}
#endif /* CONFIG_AP_PWRSEQ_S0IX */

#if CONFIG_AP_PWRSEQ_HOST_SLEEP
#define HOST_SLEEP_EVENT_DEFAULT_RESET 0

void ap_power_reset_host_sleep_state(void)
{
	power_set_host_sleep_state(HOST_SLEEP_EVENT_DEFAULT_RESET);
	sleep_reset_tracking();
	ap_power_chipset_handle_host_sleep_event(
			HOST_SLEEP_EVENT_DEFAULT_RESET, NULL);
}

/* TODO: hook to reset event */
void ap_power_handle_chipset_reset(void)
{
	if (ap_power_in_state(AP_POWER_STATE_STANDBY))
		ap_power_reset_host_sleep_state();
}

void ap_power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx)
{
	LOG_DBG("host sleep event = %d!", state);
#if CONFIG_AP_PWRSEQ_S0IX
	if (state == HOST_SLEEP_EVENT_S0IX_SUSPEND) {

		/*
		 * Indicate to power state machine that a new host event for
		 * s0ix/s3 suspend has been received and so chipset suspend
		 * notification needs to be sent to listeners.
		 */
		ap_power_sleep_set_notify(AP_POWER_SLEEP_SUSPEND);
		sleep_start_suspend(ctx);
		power_signal_enable(PWR_SLP_S0);

	} else if (state == HOST_SLEEP_EVENT_S0IX_RESUME) {
		/*
		 * Set sleep state to resume; restore SCI/SMI masks;
		 * SLP_S0 should be de-asserted already, disable interrupt.
		 */
		ap_power_sleep_set_notify(AP_POWER_SLEEP_RESUME);
		power_s0ix_resume_restore_masks();
		power_signal_disable(PWR_SLP_S0);
		sleep_complete_resume(ctx);

		/*
		 * If the sleep signal timed out and never transitioned, then
		 * the wake mask was modified to its suspend state (S0ix), so
		 * that the event wakes the system. Explicitly restore the wake
		 * mask to its S0 state now.
		 */
		power_update_wake_mask();

	} else if (state == HOST_SLEEP_EVENT_DEFAULT_RESET) {
		power_signal_disable(PWR_SLP_S0);
	}
#endif /* CONFIG_AP_PWRSEQ_S0IX */
}

#ifdef CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY

static uint16_t sleep_signal_timeout;
static uint16_t host_sleep_timeout_default = CONFIG_SLEEP_TIMEOUT_MS;
static uint32_t sleep_signal_transitions;
static enum sleep_hang_type timeout_hang_type;

static void sleep_transition_timeout(struct k_work *work);

__overridable void power_board_handle_sleep_hang(enum sleep_hang_type hang_type)
{
	/* Default empty implementation */
}

__overridable void
power_chipset_handle_sleep_hang(enum sleep_hang_type hang_type)
{
	/*
	 * Wake up the AP so they don't just chill in a non-suspended state and
	 * burn power. Overload a vaguely related event bit since event bits are
	 * at a premium. If the system never entered S0ix, then manually set the
	 * wake mask to pretend it did, so that the hang detect event wakes the
	 * system.
	 */
	if (pwr_sm_get_state() == SYS_POWER_STATE_S0) {
		host_event_t sleep_wake_mask;

		ap_power_get_lazy_wake_mask(SYS_POWER_STATE_S0ix,
							&sleep_wake_mask);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, sleep_wake_mask);
	}

	ccprintf("Warning: Detected sleep hang! Waking host up!");
	host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
}

static K_WORK_DELAYABLE_DEFINE(
	sleep_transition_timeout_data, sleep_transition_timeout);

static void sleep_transition_timeout(struct k_work *work)
{
	/* Mark the timeout. */
	sleep_signal_transitions |= EC_HOST_RESUME_SLEEP_TIMEOUT;
	k_work_cancel_delayable(&sleep_transition_timeout_data);

	if (timeout_hang_type != SLEEP_HANG_NONE) {
		power_chipset_handle_sleep_hang(timeout_hang_type);
		power_board_handle_sleep_hang(timeout_hang_type);
	}
}

static void sleep_increment_transition(void)
{
	if ((sleep_signal_transitions & EC_HOST_RESUME_SLEEP_TRANSITIONS_MASK) <
	    EC_HOST_RESUME_SLEEP_TRANSITIONS_MASK)
		sleep_signal_transitions += 1;
}

void sleep_suspend_transition(void)
{
	sleep_increment_transition();
	k_work_cancel_delayable(&sleep_transition_timeout_data);
}

void sleep_resume_transition(void)
{
	sleep_increment_transition();

	/*
	 * Start the timer again to ensure the AP doesn't get itself stuck in
	 * a state where it's no longer in a sleep state (S0ix/S3), but from
	 * the Linux perspective is still suspended. Perhaps a bug in the SoC-
	 * internal periodic housekeeping code might result in a situation
	 * like this.
	 */
	if (sleep_signal_timeout) {
		timeout_hang_type = SLEEP_HANG_S0IX_RESUME;
		k_work_schedule(&sleep_transition_timeout_data,
					K_MSEC(sleep_signal_timeout));
	}
}

void sleep_start_suspend(struct host_sleep_event_context *ctx)
{
	uint16_t timeout = ctx->sleep_timeout_ms;

	sleep_signal_transitions = 0;

	/* Use zero internally to indicate no timeout. */
	if (timeout == EC_HOST_SLEEP_TIMEOUT_DEFAULT) {
		timeout = host_sleep_timeout_default;
	}

	/* Use 0xFFFF to disable the timeout */
	if (timeout == EC_HOST_SLEEP_TIMEOUT_INFINITE) {
		sleep_signal_timeout = 0;
		return;
	}

	sleep_signal_timeout = timeout;
	timeout_hang_type = SLEEP_HANG_S0IX_SUSPEND;
	k_work_schedule(&sleep_transition_timeout_data, K_MSEC(timeout));
}

void sleep_complete_resume(struct host_sleep_event_context *ctx)
{
	/*
	 * Ensure we don't schedule another sleep_transition_timeout
	 * if the the HOST_SLEEP_EVENT_S0IX_RESUME message arrives before
	 * the CHIPSET task transitions to the POWER_S0ixS0 state.
	 */
	sleep_signal_timeout = 0;
	k_work_cancel_delayable(&sleep_transition_timeout_data);
	ctx->sleep_transitions = sleep_signal_transitions;
}

void sleep_reset_tracking(void)
{
	sleep_signal_transitions = 0;
	sleep_signal_timeout = 0;
	timeout_hang_type = SLEEP_HANG_NONE;
}

#else /* !CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY */

/* No action */
void sleep_suspend_transition(void)
{
}

void sleep_resume_transition(void)
{
}

void sleep_start_suspend(struct host_sleep_event_context *ctx)
{
}

void sleep_complete_resume(struct host_sleep_event_context *ctx)
{
}

void sleep_reset_tracking(void)
{
}

#endif /* CONFIG_AP_PWRSEQ_S0IX_ERROR_RECOVERY */

#endif /* CONFIG_AP_PWRSEQ_HOST_SLEEP */
