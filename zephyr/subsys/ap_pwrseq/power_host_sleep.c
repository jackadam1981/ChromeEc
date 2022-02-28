/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <ap_power/ap_power_interface.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

#ifdef CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI

/* If host doesn't program s0ix lazy wake mask, use default s0ix mask */
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
	enum power_state shim_state;

	state = pwr_sm_get_state();
	shim_state = convert_ap_power_state_to_shim(state);

	if (state == SYS_POWER_STATE_S0)
		wake_mask = 0;
	else if (lpc_is_active_wm_set_by_host())
		return;
	else if (get_lazy_wake_mask(shim_state, &wake_mask))
		return;
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
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

void power_set_active_wake_mask(void)
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
	if (rv < 0)
		LOG_ERR("Work queue error");
}

#else /* CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI */
static void power_set_active_wake_mask(void) { }
#endif /* CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI */

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
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
/*
 * Clear host event masks for SMI and SCI when host is entering S0ix. This is
 * done to prevent any SCI/SMI interrupts when the host is in suspend. Since
 * BIOS is not involved in the suspend path, EC needs to take care of clearing
 * these masks.
 */
static void lpc_s0ix_suspend_clear_masks(void)
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
static void lpc_s0ix_resume_restore_masks(void)
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

static void lpc_s0ix_hang_detected(void)
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

		get_lazy_wake_mask(POWER_S0ix, &sleep_wake_mask);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, sleep_wake_mask);
	}
	LOG_INF("Warning: Detected sleep hang! Waking host up!");
	host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
}

void handle_s0ix_in_chipset_suspend(void)
{
	/* Clear masks before any hooks are run for suspend. */
	lpc_s0ix_suspend_clear_masks();
}

void power_reset_host_sleep_state(void)
{
	power_set_host_sleep_state(HOST_SLEEP_EVENT_DEFAULT_RESET);
	sleep_reset_tracking();
	power_chipset_handle_host_sleep_event(HOST_SLEEP_EVENT_DEFAULT_RESET,
					      NULL);
}
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_S0IX */

void ap_pwrseq_handle_chipset_reset(void)
{
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	if (ap_power_in_state(AP_POWER_STATE_STANDBY))
		power_reset_host_sleep_state();
#endif
}

#ifdef CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP

void power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx)
{
#ifdef CONFIG_PLATFORM_EC_POWERSEQ_S0IX
	if (state == HOST_SLEEP_EVENT_S0IX_SUSPEND) {

		/*
		 * Indicate to power state machine that a new host event for
		 * s0ix/s3 suspend has been received and so chipset suspend
		 * notification needs to be sent to listeners.
		 */
		sleep_set_notify(SLEEP_NOTIFY_SUSPEND);
		sleep_start_suspend(ctx, lpc_s0ix_hang_detected);
		power_signal_enable_interrupt(PWR_SLP_S0);

	} else if (state == HOST_SLEEP_EVENT_S0IX_RESUME) {
		/*
		 * Wake up chipset task and indicate to power state machine that
		 * listeners need to be notified of chipset resume.
		 */
		sleep_set_notify(SLEEP_NOTIFY_RESUME);
		lpc_s0ix_resume_restore_masks();
		power_signal_disable_interrupt(PWR_SLP_S0);
		sleep_complete_resume(ctx);
		/*
		 * If the sleep signal timed out and never transitioned, then
		 * the wake mask was modified to its suspend state (S0ix), so
		 * that the event wakes the system. Explicitly restore the wake
		 * mask to its S0 state now.
		 */
		power_update_wake_mask();

	} else if (state == HOST_SLEEP_EVENT_DEFAULT_RESET) {
		/* TODO: undo comment */
		/* power_signal_disable_interrupt(PWR_SLP_S0); */
	}
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_S0IX */

}
#endif /* CONFIG_PLATFORM_EC_POWERSEQ_HOST_SLEEP */

