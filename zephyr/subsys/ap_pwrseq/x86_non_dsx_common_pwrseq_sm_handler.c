/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/init.h>

#include <x86_non_dsx_common_pwrseq_sm_handler.h>

#include <ap_power/ap_pwrseq_sm.h>
#define DT_DRV_COMPAT        ap_pwrseq_state

static const struct device * ap_pwrseq_dev = DEVICE_DT_INST_GET(0);

static struct pwrseq_context pwrseq_ctx;
/* S5 inactive timer*/

void s5_inactive_timer_fn(struct k_timer *timer);

K_TIMER_DEFINE(s5_inactive_timer, s5_inactive_timer_fn, NULL);

LOG_MODULE_REGISTER(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

/*
 * Returns true if all signals in mask are valid.
 * This is only done for virtual wire signals.
 */
static inline bool signals_valid(power_signal_mask_t signals)
{
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S3)
	if ((signals & POWER_SIGNAL_MASK(PWR_SLP_S3)) &&
	    power_signal_get(PWR_SLP_S3) < 0)
		return false;
#endif
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S4)
	if ((signals & POWER_SIGNAL_MASK(PWR_SLP_S4)) &&
	    power_signal_get(PWR_SLP_S4) < 0)
		return false;
#endif
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S5)
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

void s5_inactive_timer_fn(struct k_timer *timer)
{
	ap_pwrseq_post_event(ap_pwrseq_dev, AP_PWRSEQ_EVENT_POWER_TIMEOUT);
}

const char * const pwr_sm_get_state_name(enum ap_pwrseq_state state)
{
	return ap_pwrseq_get_state_str(state);
}

void pwr_sm_set_state(enum ap_pwrseq_state new_state)
{
	/* Add locking mechanism if multiple thread can update it */
	LOG_DBG("Power state: %s --> %s",
		pwr_sm_get_state_name(pwrseq_ctx.power_state),
		pwr_sm_get_state_name(new_state));
	pwrseq_ctx.power_state = new_state;
}

void request_exit_hardoff(bool should_exit)
{
	pwrseq_ctx.want_g3_exit = should_exit;
}

void ap_power_force_shutdown(enum ap_power_shutdown_reason reason)
{
	board_ap_power_force_shutdown();
}

static void shutdown_and_notify(enum ap_power_shutdown_reason reason)
{
	ap_power_force_shutdown(reason);
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN);
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN_COMPLETE);
}

void set_reboot_ap_at_g3_delay_seconds(uint32_t d_time)
{
	pwrseq_ctx.reboot_ap_at_g3_delay_ms = d_time * MSEC;
}

enum ap_pwrseq_state pwr_sm_get_state(void)
{
	enum ap_pwrseq_state state;

	ap_pwrseq_get_current_state(ap_pwrseq_dev, &state);
	return state;
}

void apshutdown(void)
{
	ap_pwrseq_sm_set_state((struct ap_pwrseq_sm_data*) ap_pwrseq_dev, AP_POWER_STATE_G3);
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

/* Check RSMRST is fine to move from S5 to higher state */
int rsmrst_power_is_good(void)
{
	/* TODO: Check if this is still intact */
	return power_signal_get(PWR_RSMRST);
}

/* Handling RSMRST signal is mostly common across x86 chipsets */
void rsmrst_pass_thru_handler(void)
{
	/* Handle RSMRST passthrough */
	/* TODO: Add additional conditions for RSMRST handling */
	int in_sig_val = power_signal_get(PWR_RSMRST);
	int out_sig_val = power_signal_get(PWR_EC_PCH_RSMRST);

	if (in_sig_val != out_sig_val) {
		if (in_sig_val)
			k_msleep(AP_PWRSEQ_DT_VALUE(rsmrst_delay));
		LOG_DBG("Setting PWR_EC_PCH_RSMRST to %d", in_sig_val);
		power_signal_set(PWR_EC_PCH_RSMRST, in_sig_val);
	}
}

static void x86_non_dsx_g3_entry(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;
	enum ap_pwrseq_state prev_state = ap_pwrseq_sm_get_prev_state(data);

	if (prev_state != AP_POWER_STATE_UNDEF) {
		/*
		 * This is not initialization since we are coming from a
		 * valid power state.
		 */
		shutdown_and_notify(AP_POWER_SHUTDOWN_G3);
	}
	switch (prev_state) {
	case AP_POWER_STATE_S5:
		ap_power_ev_send_callbacks(AP_POWER_HARD_OFF);
		break;

	case AP_POWER_STATE_S4:
	case AP_POWER_STATE_S3:
		shutdown_and_notify(AP_POWER_SHUTDOWN_POWERFAIL);
		break;
	default:
		break;
	}
}

static void x86_non_dsx_g3_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	EXIT_IF_NOT_READY(data);
	/*
	 * G3->S0 transition should happen only after the
	 * user specified delay. Hence, wait until the
	 * user specified delay times out.
	 */
	if (pwrseq_ctx.reboot_ap_at_g3_delay_ms) {
		k_msleep(pwrseq_ctx.reboot_ap_at_g3_delay_ms);
		pwrseq_ctx.reboot_ap_at_g3_delay_ms = 0;
	}

	if ((power_get_signals() & PWRSEQ_G3S5_UP_SIGNAL) ==
	    PWRSEQ_G3S5_UP_VALUE) {
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
	}
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_G3,
			   x86_non_dsx_g3_entry,
			   x86_non_dsx_g3_run,
			   NULL)


static void x86_non_dsx_s5_entry(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;
	enum ap_pwrseq_state prev_state = ap_pwrseq_sm_get_prev_state(data);

	if (prev_state != AP_POWER_STATE_G3) {
		return;
	}
	if (AP_PWRSEQ_DT_VALUE(s5_inactivity_timeout) != 0) {
		/* Timer is not started or stopped */
		k_timer_start(&s5_inactive_timer,
			K_SECONDS(AP_PWRSEQ_DT_VALUE(
			s5_inactivity_timeout)),
			K_NO_WAIT);
	}
}

static void x86_non_dsx_s5_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;
	/* In S5 make sure no more signal lost */
	/* If A-rails are stable then move to higher state */

	/* At this point, board should have already checked all power rails */
	if(IS_READY(data) && rsmrst_power_is_good()) {
		/* rsmrst is intact */
		rsmrst_pass_thru_handler();
		if (signals_valid_and_off(IN_PCH_SLP_S5)) {
			k_timer_stop(&s5_inactive_timer);
			ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
			return;
		}
	}
	/* S5 inactivity timeout, go to G3 */
	if (AP_PWRSEQ_DT_VALUE(s5_inactivity_timeout) == 0)
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	else if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_TIMEOUT)) {
		/* Timer is expired */
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
	}
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S5,
			   x86_non_dsx_s5_entry,
			   x86_non_dsx_s5_run,
			   NULL)

static void x86_non_dsx_s4_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	EXIT_IF_NOT_READY(data);

	if (signals_valid_and_on(IN_PCH_SLP_S5)) {
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S5);
		return;
	}
	else if (signals_valid_and_off(IN_PCH_SLP_S4)) {
		/* Notify power event that rails are up */
		ap_power_ev_send_callbacks(AP_POWER_STARTUP);
#if CONFIG_AP_PWRSEQ_S0IX
		/*
		 * Clearing the S0ix flag on the path to S0
		 * to handle any reset conditions.
		 */
		ap_power_reset_host_sleep_state();
#endif
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
		return;
	}
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S4,
			   NULL,
			   x86_non_dsx_s4_run,
			   NULL)

static void x86_non_dsx_s3_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	EXIT_IF_NOT_READY(data);

	if (!power_signals_on(IN_PGOOD_ALL_CORE)) {
		/* Required rail went away */
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
		return;
	}

	if (signals_valid_and_on(IN_PCH_SLP_S4)) {
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
	}

	if (signals_valid_and_on(IN_PCH_SLP_S3)) {
		return;
	}

	/* All the power rails must be stable */
	if (power_signal_get(PWR_ALL_SYS_PWRGD)) {
#if CONFIG_PLATFORM_EC_CHIPSET_RESUME_INIT_HOOK
		/* Notify power event before resume */
		ap_power_ev_send_callbacks(AP_POWER_RESUME_INIT);
#endif
		/* Notify power event rails are up */
		ap_power_ev_send_callbacks(AP_POWER_RESUME);
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S0);
	}
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S3,
			   NULL,
			   x86_non_dsx_s3_run,
			   NULL)

static void x86_non_dsx_s0_run(void *arg)
{
	struct ap_pwrseq_sm_data *data = arg;

	EXIT_IF_NOT_READY(data);

	if (!power_signals_on(IN_PGOOD_ALL_CORE)) {
		/* Required rail went away */
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_G3);
		return;
	}

	if (signals_valid_and_on(IN_PCH_SLP_S3)) {
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S3);
	} else if (signals_valid_and_on(IN_PCH_SLP_S4)) {
		ap_pwrseq_sm_set_state(data, AP_POWER_STATE_S4);
	}
}

AP_POWER_ARCH_STATE_DEFINE(AP_POWER_STATE_S0,
			   NULL,
			   x86_non_dsx_s0_run,
			   NULL)
