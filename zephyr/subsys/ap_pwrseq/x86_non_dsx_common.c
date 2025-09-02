/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ap_reset_log.h"
#include "system_boot_time.h"
#include "x86_non_dsx_common_pwrseq_sm_handler.h"
#include "zephyr_console_shim.h"

#ifdef CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND
static bool in_debug_mode;
#endif

LOG_MODULE_DECLARE(ap_pwrseq, CONFIG_AP_PWRSEQ_LOG_LEVEL);

void ap_power_force_shutdown(enum ap_power_shutdown_reason reason)
{
	/* This prevents force shutdown if debug mode is enabled */
	if (ap_power_in_debug_mode()) {
		LOG_WRN("debug_mode is enabled, preventing force shutdown");
		return;
	}

	report_ap_reset((enum chipset_shutdown_reason)reason);

	board_ap_power_force_shutdown();
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

	report_ap_reset((enum chipset_shutdown_reason)reason);

	power_signal_set(PWR_SYS_RST, 1);
	/*
	 * Debounce time for SYS_RESET_L is 16 ms. Wait twice that period
	 * to be safe.
	 */
	k_msleep(AP_PWRSEQ_DT_VALUE(sys_reset_delay));
	power_signal_set(PWR_SYS_RST, 0);
	ap_power_ev_send_callbacks(AP_POWER_RESET);
}

int rsmrst_power_is_good(void)
{
	/* TODO: Check if this is still intact */
	return power_signal_get(PWR_RSMRST_PWRGD);
}

/* Handling RSMRST signal is mostly common across x86 chipsets */
void rsmrst_pass_thru_handler(void)
{
	/* Handle RSMRST passthrough */
	/* TODO: Add additional conditions for RSMRST handling */
	if (power_signal_get(PWR_RSMRST_PWRGD)) {
		if (power_signal_get(PWR_EC_PCH_RSMRST)) {
			/*
			 * Delay `PWR_EC_PCH_RSMRST` de-assertion for at least
			 * `rsmrst_delay` after detecting that power wells are
			 * stable.
			 */
			k_msleep(AP_PWRSEQ_DT_VALUE(rsmrst_delay));
			LOG_DBG("Deasserting PWR_EC_PCH_RSMRST");
			power_signal_set(PWR_EC_PCH_RSMRST, 0);
			update_ap_boot_time(RSMRST);
		}
	} else {
		power_signal_set(PWR_EC_PCH_RSMRST, 1);
	}
}

bool ap_power_in_debug_mode(void)
{
#ifdef CONFIG_AP_PWRSEQ_DEBUG_MODE_COMMAND
	return in_debug_mode;
#else
	return false;
#endif
}

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
