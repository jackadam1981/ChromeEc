/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Common X86 chipset power control module for Chrome EC */

#include "power/common_x86.h"
#include "charge_state.h"

#ifndef CHARGER_INITIALIZED_TRIES
#define CHARGER_INITIALIZED_TRIES 40
#endif

#ifndef CHARGER_INITIALIZED_DELAY_MS
#define CHARGER_INITIALIZED_DELAY_MS 100
#endif

#ifndef GPIO_PG_EC_RSMRST_ODL
#define GPIO_PG_EC_RSMRST_ODL GPIO_PCH_RSMRST_L
#endif

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ##args)

int power_s5_up;       /* Chipset is sequencing up or down */

void x86_rsmrst_signal_interrupt(enum gpio_signal signal)
{
	int rsmrst_in = gpio_get_level(GPIO_PG_EC_RSMRST_ODL);
	int rsmrst_out = gpio_get_level(GPIO_PCH_RSMRST_L);

	/*
	 * This function is called when rsmrst changes state. If rsmrst
	 * has been asserted (high -> low) then pass this new state to PCH.
	 */
	if (!rsmrst_in && (rsmrst_in != rsmrst_out))
		gpio_set_level(GPIO_PCH_RSMRST_L, rsmrst_in);

	/*
	 * Call the main power signal interrupt handler to wake up the chipset
	 * task which handles low->high rsmrst pass through.
	 */
	power_signal_interrupt(signal);
}

__overridable void board_before_rsmrst(int rsmrst)
{
}

__overridable void board_after_rsmrst(int rsmrst)
{
}

#ifdef CONFIG_CHARGER
/* Flag to indicate if power up was inhibited due to low battery SOC level. */
static int power_up_inhibited;

/*
 * Check if AP power up should be inhibited.
 * 0 = Ok to boot up AP
 * 1 = AP power up is inhibited.
 */
static int is_power_up_inhibited(void)
{
	/* Defaulting to power button not pressed. */
	const int power_button_pressed = 0;

	return charge_prevent_power_on(power_button_pressed) ||
		charge_want_shutdown();
}

void power_up_inhibited_cb(void)
{
	if (!power_up_inhibited)
		return;

	if (is_power_up_inhibited()) {
		CPRINTS("power-up still inhibited");
		return;
	}

	CPRINTS("Battery SOC ok to boot AP!");
	power_up_inhibited = 0;

	chipset_exit_hard_off();
}
#endif

enum ec_error_list x86_wait_power_up_ok(void)
{
#ifdef CONFIG_CHARGER
	int tries = 0;

	/*
	 * Allow charger to be initialized for up to defined tries,
	 * in case we're trying to boot the AP with no battery.
	 */
	while ((tries < CHARGER_INITIALIZED_TRIES) &&
	       is_power_up_inhibited()) {
		msleep(CHARGER_INITIALIZED_DELAY_MS);
		tries++;
	}

	/*
	 * Return to G3 if battery level is too low. Set
	 * power_up_inhibited in order to check the eligibility to boot
	 * AP up after battery SOC changes.
	 */
	if (tries == CHARGER_INITIALIZED_TRIES) {
		CPRINTS("power-up inhibited");
		power_up_inhibited = 1;
		return EC_ERROR_TIMEOUT;
	}

	power_up_inhibited = 0;
#endif

	if (IS_ENABLED(CONFIG_VBOOT_EFS) || IS_ENABLED(CONFIG_VBOOT_EFS2)) {
		/*
		 * We have to test power readiness here (instead of S5->S3)
		 * because when entering S5, EC enables EC_ROP_SLP_SUS pin
		 * which causes (short-powered) system to brown out.
		 */
		while (!system_can_boot_ap())
			msleep(200);
	}
	return EC_SUCCESS;
}

__overridable bool is_passthrough_valid(enum gpio_signal pin_in,
	enum gpio_signal pin_out, int *p_in_level)
{
#if defined(CONFIG_CHIPSET_X86_RSMRST_DELAY)
	/*
	 * Wait at least 10ms between power signals going high
	 * and deasserting RSMRST to PCH.
	 */
	if (*p_in_level)
		msleep(10);
#endif
	return true;
}

void handle_pass_through_with_callbacks(enum gpio_signal pin_in,
	enum gpio_signal pin_out,
	void (*before_write_callback)(int),
	void (*after_write_callback)(int))
{
	/*
	 * Pass through asynchronously, as SOC may not react
	 * immediately to power changes.
	 */
	int in_level = gpio_get_level(pin_in);
	int out_level = gpio_get_level(pin_out);

	/* Nothing to do. */
	if (in_level == out_level)
		return;

	if (before_write_callback != NULL)
		before_write_callback(in_level);

	if (!is_passthrough_valid(pin_in, pin_out, &in_level))
		return;

	gpio_set_level(pin_out, in_level);

	CPRINTS("Pass through %s: %d", gpio_get_name(pin_in), in_level);
	if (after_write_callback != NULL)
		after_write_callback(in_level);
}

void handle_pass_through(enum gpio_signal pin_in,
		enum gpio_signal pin_out)
{
	handle_pass_through_with_callbacks(pin_in, pin_out, NULL, NULL);
}

void chipset_throttle_cpu(int throttle)
{
	CPRINTS("%s(%d)", __func__, throttle);

	if (IS_ENABLED(CONFIG_CPU_PROCHOT_ACTIVE_LOW))
		throttle = !throttle;

	if (chipset_in_state(CHIPSET_STATE_ON))
		gpio_set_level(GPIO_CPU_PROCHOT, throttle);
}

__overridable enum power_state chipset_force_g3(void)
{
	chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);

	return POWER_G3;
}

enum power_state power_chipset_init(void)
{
	CPRINTS("%s: power_signal=0x%x", __func__, power_get_signals());

	if (!system_jumped_to_this_image())
		return POWER_G3;
	/*
	 * We are here as RW. We need to handle the following cases:
	 *
	 * 1. Late sysjump by software sync. AP is in S0.
	 * 2. Shutting down in recovery mode then sysjump by EFS2. AP is in S5
	 *    and expected to sequence down.
	 * 3. Rebooting from recovery mode then sysjump by EFS2. AP is in S5
	 *    and expected to sequence up.
	 * 4. RO jumps to RW from main() by EFS2. (a.k.a. power on reset, cold
	 *    reset). AP is in G3.
	 */
	if ((power_get_signals() & IN_ALL_S0) == IN_ALL_S0) {
		/* case #1. Disable idle task deep sleep when in S0. */
		disable_sleep(SLEEP_MASK_AP_RUN);
		CPRINTS("already in S0");
		return POWER_S0;
	}
	if ((power_get_signals() & CHIPSET_G3S5_POWERUP_SIGNAL)
			== CHIPSET_G3S5_POWERUP_SIGNAL) {
		/* case #2 & #3 */
		CPRINTS("already in S5");
		return POWER_S5;
	}
	/* case #4 */
	chipset_force_g3();
	return POWER_G3;
}

__overridable void x86_sys_reset_delay(void)
{
	/*
	 * Debounce time for SYS_RESET_L is 16 ms. Wait twice that period
	 * to be safe.
	 */
	udelay(32 * MSEC);
}

void chipset_reset(enum chipset_shutdown_reason reason)
{
	CPRINTS("%s: %d", __func__, reason);

	/*
	 * Toggling SYS_RESET_L will not have any impact when it's already
	 * low (i,e. Chipset is in reset state).
	 */
	if (gpio_get_level(GPIO_SYS_RESET_L) == 0) {
		CPRINTS("Chipset is in reset state");
		return;
	}

	if (chipset_in_state(CHIPSET_STATE_ANY_OFF)) {
		CPRINTS("Can't reset: SOC is off");
		return;
	}

	report_ap_reset(reason);
	/*
	 * Send a pulse to SYS_RST to trigger a warm reset.
	 */
	gpio_set_level(GPIO_SYS_RESET_L, 0);
	x86_sys_reset_delay();
	gpio_set_level(GPIO_SYS_RESET_L, 1);
}

#ifdef CONFIG_POWER_S0IX
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
void lpc_s0ix_resume_restore_masks(void)
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

void lpc_s0ix_hang_detected(void)
{
	/*
	 * Wake up the AP so they don't just chill in a non-suspended state and
	 * burn power. Overload a vaguely related event bit since event bits are
	 * at a premium. If the system never entered S0ix, then manually set the
	 * wake mask to pretend it did, so that the hang detect event wakes the
	 * system.
	 */
	if (power_get_state() == POWER_S0) {
		host_event_t sleep_wake_mask;

		get_lazy_wake_mask(POWER_S0ix, &sleep_wake_mask);
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, sleep_wake_mask);
	}

	CPRINTS("Warning: Detected sleep hang! Waking host up!");
	host_set_single_event(EC_HOST_EVENT_HANG_DETECT);
}

static void handle_chipset_suspend(void)
{
	/* Clear masks before any hooks are run for suspend. */
	lpc_s0ix_suspend_clear_masks();
}

DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, handle_chipset_suspend, HOOK_PRIO_FIRST);

static void handle_chipset_reset(void)
{
	if (chipset_in_state(CHIPSET_STATE_STANDBY)) {
		CPRINTS("chipset reset: exit s0ix");
		power_reset_host_sleep_state();
		task_wake(TASK_ID_CHIPSET);
	}
}
DECLARE_HOOK(HOOK_CHIPSET_RESET, handle_chipset_reset, HOOK_PRIO_FIRST);

void power_reset_host_sleep_state(void)
{
	power_set_host_sleep_state(HOST_SLEEP_EVENT_DEFAULT_RESET);
	sleep_reset_tracking();
	power_chipset_handle_host_sleep_event(HOST_SLEEP_EVENT_DEFAULT_RESET,
						  NULL);
}

#endif /* CONFIG_POWER_S0IX */

#ifdef CONFIG_POWER_TRACK_HOST_SLEEP_STATE

__overridable void power_board_handle_host_sleep_event(
		enum host_sleep_event state)
{
	/* Default weak implementation -- no action required. */
}

__override void power_chipset_handle_host_sleep_event(
		enum host_sleep_event state,
		struct host_sleep_event_context *ctx)
{
	power_board_handle_host_sleep_event(state);

#ifdef CONFIG_POWER_S0IX
	if (state == HOST_SLEEP_EVENT_S0IX_SUSPEND) {
		/*
		 * Indicate to power state machine that a new host event for
		 * s0ix/s3 suspend has been received and so chipset suspend
		 * notification needs to be sent to listeners.
		 */
		sleep_set_notify(SLEEP_NOTIFY_SUSPEND);

		sleep_start_suspend(ctx, lpc_s0ix_hang_detected);
		power_signal_enable_interrupt(sleep_sig[SYS_SLEEP_S0IX]);
	} else if (state == HOST_SLEEP_EVENT_S0IX_RESUME) {
		/*
		 * Wake up chipset task and indicate to power state machine that
		 * listeners need to be notified of chipset resume.
		 */
		sleep_set_notify(SLEEP_NOTIFY_RESUME);
		task_wake(TASK_ID_CHIPSET);
		lpc_s0ix_resume_restore_masks();
		power_signal_disable_interrupt(sleep_sig[SYS_SLEEP_S0IX]);
		sleep_complete_resume(ctx);
		/*
		 * If the sleep signal timed out and never transitioned, then
		 * the wake mask was modified to its suspend state (S0ix), so
		 * that the event wakes the system. Explicitly restore the wake
		 * mask to its S0 state now.
		 */
		power_update_wake_mask();
	} else if (state == HOST_SLEEP_EVENT_DEFAULT_RESET) {
		power_signal_disable_interrupt(sleep_sig[SYS_SLEEP_S0IX]);
	}
#endif /* CONFIG_POWER_S0IX */
}
#endif /* CONFIG_POWER_TRACK_HOST_SLEEP_STATE */
