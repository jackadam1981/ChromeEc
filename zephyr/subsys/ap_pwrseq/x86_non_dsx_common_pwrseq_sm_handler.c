/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "zephyr_console_shim.h"

#include <zephyr/init.h>

#include <atomic.h>
#include <ap_power/ap_power_events.h>
#include "x86_non_dsx_common_pwrseq_sm_handler.h"
#include "ap_power/ap_pwrseq.h"
#include "ap_power/ap_pwrseq_sm.h"
#include "zephyr_console_shim.h"

static void x86_non_dsx_timer_handler(struct k_timer *timer);

K_TIMER_DEFINE(x86_non_dsx_timer, x86_non_dsx_timer_handler, NULL);
/*
 * Flags, may be set/cleared from other threads.
 */
enum {
	S5_INACTIVE_TIMER_RUNNING,
	START_FROM_G3,
	FLAGS_MAX,
};
static ATOMIC_DEFINE(flags, FLAGS_MAX);
/* Delay in ms when starting from G3 */
static uint32_t start_from_g3_delay_ms;

#ifdef CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND
static bool in_debug_mode;
#endif

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

/*
 * Returns true if all signals in mask are valid.
 * This is only done for virtual wire signals.
 */
static inline bool signals_valid(power_signal_mask_t signals)
{
#if defined(CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI_VW_SLP_S3)
	if ((signals & POWER_SIGNAL_MASK(PWR_SLP_S3)) &&
	    power_signal_get(PWR_SLP_S3) < 0)
		return false;
#endif
#if defined(CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI_VW_SLP_S4)
	if ((signals & POWER_SIGNAL_MASK(PWR_SLP_S4)) &&
	    power_signal_get(PWR_SLP_S4) < 0)
		return false;
#endif
#if defined(CONFIG_PLATFORM_EC_HOST_INTERFACE_ESPI_VW_SLP_S5)
	if ((signals & POWER_SIGNAL_MASK(PWR_SLP_S5)) &&
	    power_signal_get(PWR_SLP_S5) < 0)
		return false;
#endif
	return true;
}

static inline bool signals_valid_and_on(power_signal_mask_t signals)
{
	return signals_valid(signals) && power_signals_on(signals);
}

static inline bool signals_valid_and_off(power_signal_mask_t signals)
{
	return signals_valid(signals) && power_signals_off(signals);
}

const char * const pwr_sm_get_state_name(enum ap_pwrseq_state state)
{
	return ap_pwrseq_get_state_str(state);
}

/*
 * Set a flag to enable starting the AP once it is in G3.
 * This is called from ap_power_exit_hardoff() which checks
 * to ensure that the AP is in S5 or G3 state before calling
 * this function.
 * It can also be called via a hostcmd, which allows the flag
 * to be set in any AP state.
 */
void request_start_from_g3(void)
{
	LOG_INF("Request start from G3");
	atomic_set_bit(flags, START_FROM_G3);
	/*
	 * If in S5, restart the timer to give the CPU more time
	 * to respond to a power button press (which is presumably
	 * why we are being called). This avoids having the S5
	 * inactivity timer expiring before the AP can process
	 * the power button press and start up.
	 */
	if (pwr_sm_get_state() == AP_POWER_STATE_S5) {
		k_timer_stop(&x86_non_dsx_timer);
		atomic_clear_bit(flags, S5_INACTIVE_TIMER_RUNNING);
	}
	ap_pwrseq_post_event(ap_pwrseq_get_instance(),
			     AP_PWRSEQ_EVENT_POWER_STARTUP);
}

void ap_power_force_shutdown(enum ap_power_shutdown_reason reason)
{
#ifdef CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND
	/* This prevents force shutdown if debug mode is enabled */
	if (in_debug_mode) {
		LOG_WRN("debug_mode is enabled, preventing force shutdown");
		return;
	}
#endif /* CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND */
	board_ap_power_force_shutdown();
}

static void x86_non_dsx_timer_handler(struct k_timer *timer)
{
	ap_pwrseq_post_event(ap_pwrseq_get_instance(),
			     AP_PWRSEQ_EVENT_POWER_TIMEOUT);
}

void set_start_from_g3_delay_seconds(uint32_t d_time)
{
	start_from_g3_delay_ms = d_time * MSEC;
}

enum ap_pwrseq_state pwr_sm_get_state(void)
{
	return ap_pwrseq_get_current_state(ap_pwrseq_get_instance());
}

void apshutdown(void)
{
	if (ap_pwrseq_get_current_state(ap_pwrseq_get_instance()) !=
	    AP_POWER_STATE_G3) {
		ap_power_force_shutdown(AP_POWER_SHUTDOWN_G3);
	}
}

void ap_power_reset(enum ap_power_shutdown_reason reason)
{
	/*
	 * Irrespective of cold_reset value, always toggle SYS_RESET_L to
	 * perform an AP reset. RCIN# which was used earlier to trigger
	 * a warm reset is known to not work in certain cases where the CPU
	 * is in a bad state (crbug.com/721853).
	 *
	 * The EC cannot control warm vs cold reset of the AP using
	 * SYS_RESET_L; it's more of a request.
	 */
	LOG_DBG("%s: %d", __func__, reason);

	/*
	 * Toggling SYS_RESET_L will not have any impact when it's already
	 * low (i,e. AP is in reset state).
	 */
	if (power_signal_get(PWR_SYS_RST)) {
		LOG_DBG("Chipset is in reset state");
		return;
	}

	power_signal_set(PWR_SYS_RST, 1);
	/*
	 * Debounce time for SYS_RESET_L is 16 ms. Wait twice that period
	 * to be safe.
	 */
	k_msleep(AP_PWRSEQ_DT_VALUE(sys_reset_delay));
	power_signal_set(PWR_SYS_RST, 0);
	ap_power_ev_send_callbacks(AP_POWER_RESET);
}

/* Handling RSMRST signal is mostly common across x86 chipsets */
void rsmrst_pass_thru_handler(void)
{
	int in_sig_val = power_signal_get(PWR_RSMRST);
	int out_sig_val = power_signal_get(PWR_EC_PCH_RSMRST);

	if (in_sig_val == out_sig_val) {
		return;
	}

	if (in_sig_val) {
		k_msleep(AP_PWRSEQ_DT_VALUE(rsmrst_delay));
	}

	LOG_DBG("Setting PWR_EC_PCH_RSMRST to %d", in_sig_val);
	power_signal_set(PWR_EC_PCH_RSMRST, in_sig_val);
}

static int x86_non_dsx_g3_entry(void *data)
{
	if (atomic_test_bit(flags, START_FROM_G3)) {
		ap_pwrseq_post_event(ap_pwrseq_get_instance(),
				     AP_PWRSEQ_EVENT_POWER_STARTUP);
	}

	return 0;
}

static int x86_non_dsx_g3_run(void *data)
{
	/*
	 * If the START_FROM_G3 flag is set, begin starting
	 * the AP. There may be a delay set, so only start
	 * after that delay.
	 */
	if (!atomic_test_bit(flags, START_FROM_G3)) {
		return 0;
	}
	if (start_from_g3_delay_ms) {
		LOG_INF("Starting from G3, delay %d ms",
			start_from_g3_delay_ms);

		k_timer_start(&x86_non_dsx_timer,
			K_MSEC(start_from_g3_delay_ms),
			K_NO_WAIT);

		start_from_g3_delay_ms = 0;
		return 0;
	}

	if (k_timer_remaining_get(&x86_non_dsx_timer)) {
		return 0;
	}

	atomic_clear_bit(flags, START_FROM_G3);
	/*
	 * At this point all power rails and power signals are already checked
	 * by application and chipset state action handlers, it is safe to move
	 * forward to S5.
	 */
	return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_G3,
			   x86_non_dsx_g3_entry,
			   x86_non_dsx_g3_run,
			   NULL)

static int x86_non_dsx_s5_entry(void *data)
{
	if (AP_PWRSEQ_DT_VALUE(s5_inactivity_timeout)) {
		atomic_set_bit(flags, S5_INACTIVE_TIMER_RUNNING);
		k_timer_start(&x86_non_dsx_timer,
			K_SECONDS(AP_PWRSEQ_DT_VALUE(
			s5_inactivity_timeout)),
			K_NO_WAIT);
	}

	return 0;
}

static int x86_non_dsx_s5_run(void *data)
{
	/* At this point, board should have already checked all power rails */
	if (power_signal_get(PWR_RSMRST)) {
		rsmrst_pass_thru_handler();

		if (signals_valid_and_off(IN_PCH_SLP_S5)) {
			return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
		}
	}
	/* S5 inactivity timeout, go to G3 */
	if (AP_PWRSEQ_DT_VALUE(s5_inactivity_timeout) == 0) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	} else if (k_timer_remaining_get(&x86_non_dsx_timer) == 0) {
		/* Timer is expired */
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}

static int x86_non_dsx_s5_exit(void *data)
{
	k_timer_stop(&x86_non_dsx_timer);
	atomic_clear_bit(flags, S5_INACTIVE_TIMER_RUNNING);

	return 0;
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S5,
			   x86_non_dsx_s5_entry,
			   x86_non_dsx_s5_run,
			   x86_non_dsx_s5_exit)

static int x86_non_dsx_s4_run(void *data)
{
	if (power_signal_get(PWR_RSMRST) == 0 ||
	    signals_valid_and_on(IN_PCH_SLP_S5)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}

	if (signals_valid_and_off(IN_PCH_SLP_S4)) {
#if CONFIG_AP_PWRSEQ_S0IX
		/*
		 * Clearing the S0ix flag on the path to S0
		 * to handle any reset conditions.
		 */
		ap_power_reset_host_sleep_state();
#endif
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
	}

	return 0;
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S4,
			   NULL,
			   x86_non_dsx_s4_run,
			   NULL)

static int x86_non_dsx_s3_run(void *data)
{
	if (signals_valid_and_on(IN_PCH_SLP_S4)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
	}

	if (signals_valid_and_on(IN_PCH_SLP_S3)) {
		return 0;
	}

	/* All the power rails must be stable */
	if (power_signal_get(PWR_ALL_SYS_PWRGD)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
	}

	return 0;
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S3,
			   NULL,
			   x86_non_dsx_s3_run,
			   NULL)

static int x86_non_dsx_s0_run(void *data)
{
	if (signals_valid_and_on(IN_PCH_SLP_S3)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
	}
#if CONFIG_AP_PWRSEQ_S0IX
	if (ap_power_sleep_get_notify() == AP_POWER_SLEEP_SUSPEND &&
	    power_signals_on(IN_PCH_SLP_S0)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0IX);
	} else if (ap_power_sleep_get_notify() == AP_POWER_SLEEP_RESUME) {
		ap_power_sleep_notify_transition(AP_POWER_SLEEP_RESUME);
	}
#endif

	return 0;
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S0,
			   NULL,
			   x86_non_dsx_s0_run,
			   NULL)

#ifdef CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND
/*
 * Intel debugger puts SOC in boot halt mode for step debugging,
 * during this time EC may lose Sx lines, Adding this console
 * command to avoid force shutdown.
 */
static int disable_force_shutdown(int argc, const char **argv)
{
	if (argc > 1) {
		if (!strcmp(argv[1], "enable")) {
			in_debug_mode = true;
		} else if (!strcmp(argv[1], "disable")) {
			in_debug_mode = false;
		} else {
			return EC_ERROR_PARAM1;
		}
	}
	LOG_INF("debug_mode = %s", (in_debug_mode ? "enabled" : "disabled"));
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(debug_mode, disable_force_shutdown, "[enable|disable]",
			"Prevents force shutdown if enabled");
#endif /* CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND */

#if CONFIG_AP_PWRSEQ_S0IX
static int x86_non_dsx_s0ix_entry(void *data)
{
	/*
	 * Check sleep state and notify listeners of S0ix suspend if
	 * HC already set sleep suspend state.
	 */
	ap_power_sleep_notify_transition(AP_POWER_SLEEP_SUSPEND);
	/*
	 * Enable idle task deep sleep. Allow the low power idle task
	 * to go into deep sleep in S0ix.
	 */
	enable_sleep(SLEEP_MASK_AP_RUN);

	return 0;
}

static int x86_non_dsx_s0ix_run(void *data)
{
	/* System in S0 only if SLP_S0 and SLP_S3 are de-asserted */
	if (power_signals_off(IN_PCH_SLP_S0) &&
	    signals_valid_and_off(IN_PCH_SLP_S3)) {
		/* TODO: Make sure ap reset handling is done
		 * before leaving S0ix.
		 */
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
	} else if (!power_signals_on(IN_PGOOD_ALL_CORE)) {
		return ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}

	return 0;
}

static int x86_non_dsx_s0ix_exit(void *data)
{
	/*
	 * Disable idle task deep sleep. This means that the low
	 * power idle task will not go into deep sleep while in S0.
	 */
	disable_sleep(SLEEP_MASK_AP_RUN);

	return 0;
}

AP_POWER_CHIPSET_SUB_STATE_DEFINE(AP_POWER_STATE_S0IX,
				  x86_non_dsx_s0ix_entry,
				  x86_non_dsx_s0ix_run,
				  x86_non_dsx_s0ix_exit,
				  AP_POWER_STATE_S0)
#endif
