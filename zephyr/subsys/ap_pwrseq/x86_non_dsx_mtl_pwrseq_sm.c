/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <x86_common_pwrseq.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

void ap_off(void)
{
	power_signal_set(PWR_EC_PCH_SYS_PWROK, 0);
}

/* Generate SYS_PWROK->SOC if needed by system */
void generate_sys_pwrok_handler(void)
{
        if (power_signal_get(PWR_EC_PCH_SYS_PWROK) == 0) {
                k_msleep(AP_PWRSEQ_DT_VALUE(sys_pwrok_delay));
		/*
		 * Loop through all PWROK signals defined by the board and set
		 * to match the current ALL_SYS_PWRGD input.
		 */
		if (power_signal_get(PWR_ALL_SYS_PWRGD) == 0) {
			LOG_DBG("PG_EC_ALL_SYS_PWRGD deasserted, "
				"shutting AP off!");
			ap_off();
			return;
		}
		LOG_INF("Turning on PWR_EC_PCH_SYS_PWROK");
		power_signal_set(PWR_EC_PCH_SYS_PWROK, 1);
	}
}

/* Chipset specific power state machine handler */

void s0_action_handler(void)
{
	/* Send SYS_PWROK->SoC if conditions met */
	generate_sys_pwrok_handler();
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

void ap_power_force_shutdown(enum ap_power_shutdown_reason reason)
{
	board_ap_power_force_shutdown();
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN);
	ap_power_ev_send_callbacks(AP_POWER_SHUTDOWN_COMPLETE);
}

void s0s3_action_handler(void)
{
	ap_off();
}

void init_chipset_pwr_seq_state(void)
{
	/* Deassert reset pin */
	power_signal_set(PWR_SYS_RST, 0);
}

/**
 * Determine the current state of the CPU from the
 * power signals.
 */
enum power_states_ndsx chipset_pwr_seq_get_state(void)
{
#define MASK_ALL_POWER_GOOD \
		(POWER_SIGNAL_MASK(PWR_RSMRST) |	\
		 POWER_SIGNAL_MASK(PWR_ALL_SYS_PWRGD))
#define MASK_S0	\
	(MASK_ALL_POWER_GOOD |			\
	 POWER_SIGNAL_MASK(PWR_SLP_S0) |	\
	 POWER_SIGNAL_MASK(PWR_SLP_S3) |	\
	 POWER_SIGNAL_MASK(PWR_SLP_S4) |	\
	 POWER_SIGNAL_MASK(PWR_SLP_S5))
#define MASK_S5 \
	(MASK_ALL_POWER_GOOD |			\
	 POWER_SIGNAL_MASK(PWR_SLP_S5))

	/*
	 * Chip is shut down.
	 */
	if ((power_get_signals() & MASK_ALL_POWER_GOOD) == 0) {
		LOG_DBG("Power rails off, G3 state");
		return SYS_POWER_STATE_G3;
	}
	/*
	 * If not all the power rails are available,
	 * then force shutdown to G3 to get to known state.
	 */
	if ((power_get_signals() & MASK_ALL_POWER_GOOD)
			!= MASK_ALL_POWER_GOOD) {
		ap_power_force_shutdown(AP_POWER_SHUTDOWN_G3);
		LOG_INF("Not all power rails up, forcing shutdown");
		return SYS_POWER_STATE_G3;
	}

	/*
	 * All the power rails are good, so
	 * wait for virtual wire signals to become available.
	 * Not sure how long to wait? 5 seconds total.
	 */
	for (int delay = 0; delay < 500; k_msleep(10), delay++) {
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S3)
		if (power_signal_get(PWR_SLP_S3) < 0)
			continue;
#endif
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S4)
		if (power_signal_get(PWR_SLP_S4) < 0)
			continue;
#endif
#if defined(CONFIG_PLATFORM_EC_ESPI_VW_SLP_S5)
		if (power_signal_get(PWR_SLP_S5) < 0)
			continue;
#endif
		/*
		 * All signals valid.
		 */
		LOG_DBG("All VW signals valid after %d ms", delay * 10);
		break;
	}
	/*
	 * S0, all power OK, no suspend or sleep on.
	 */
	if ((power_get_signals() & MASK_S0) == MASK_ALL_POWER_GOOD) {
		LOG_DBG("CPU in S0 state");
		return SYS_POWER_STATE_S0;
	}
	/*
	 * S3, all power OK, PWR_SLP_S3 on.
	 */
	if ((power_get_signals() & MASK_S0) ==
		(MASK_ALL_POWER_GOOD | POWER_SIGNAL_MASK(PWR_SLP_S3))) {
		LOG_DBG("CPU in S3 state");
		return SYS_POWER_STATE_S3;
	}
	/*
	 * S5, all power OK, PWR_SLP_S5 on.
	 */
	if ((power_get_signals() & MASK_S5) == MASK_S5) {
		LOG_DBG("CPU in S5 state");
		return SYS_POWER_STATE_S5;
	}
	/*
	 * Unable to determine state, force to G3.
	 */
	ap_power_force_shutdown(AP_POWER_SHUTDOWN_G3);
	LOG_INF("Unable to determine CPU state, forcing shutdown");
	return SYS_POWER_STATE_G3;
}

enum power_states_ndsx chipset_pwr_sm_run(enum power_states_ndsx curr_state)
{
	/* Add chipset specific state handling if any */
	switch (curr_state) {
	case SYS_POWER_STATE_G3S5:
		board_ap_power_action_g3_s5();
		break;
	case SYS_POWER_STATE_S0S3:
		s0s3_action_handler();
		break;
	case SYS_POWER_STATE_S0:
		s0_action_handler();
		break;
	default:
		break;
	}
	return curr_state;
}
