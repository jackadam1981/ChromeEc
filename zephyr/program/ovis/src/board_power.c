/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "console.h"
#include "gpio/gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "system.h"
#include "system_boot_time.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power/ap_power_interface.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#if defined(CONFIG_X86_NON_DSX_PWRSEQ_MTL) || \
	defined(CONFIG_TEST_X86_NON_DSX_PWRSEQ_MTL)
#define X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS 50

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

void board_ap_power_force_shutdown(void)
{
	int timeout_ms = X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS;

	/* Assert PCH_RMSRST to meet tPCH12 */
	power_signal_set(PWR_EC_PCH_RSMRST, 1);

	/* Turn off PRIM load switch. */
	power_signal_set(PWR_EN_PP3300_A, 0);

	/* Wait RSMRST_PWRGD to de-assert. */
	while (power_signal_get(PWR_RSMRST_PWRGD) && (timeout_ms > 0)) {
		k_msleep(1);
		timeout_ms--;
	};

	if (power_signal_get(PWR_RSMRST_PWRGD)) {
		LOG_WRN("RSMRST_PWRGD still asserted!  Assuming G3.");
	}
}

void board_ap_power_action_g3_s5(void)
{
	/* Turn on the PP3300_PRIM rail. */
	power_signal_set(PWR_EN_PP3300_A, 1);

	update_ap_boot_time(ARAIL);

	if (!power_wait_signals_on_timeout(
		    POWER_SIGNAL_MASK(PWR_RSMRST_PWRGD),
		    AP_PWRSEQ_DT_VALUE(wait_signal_timeout))) {
		ap_power_ev_send_callbacks(AP_POWER_PRE_INIT);
	}
}

bool board_ap_power_check_power_rails_enabled(void)
{
	return power_signal_get(PWR_EN_PP3300_A);
}

#ifdef CONFIG_POWER_BUTTON_INIT_IDLE
/*
 * The AP_IDLE flag is expected to be set when graceful shutdown
 * (e.g. shutdown from UI). The flag is set when CHIPSET_SHUTDOWN hook
 * is called during AP power state S4 to S5 transition. On Deku, the
 * AP_IDLE flag is set unpextedly when AC power is removed. As a workaround,
 * delay the timing of setting the AP_IDLE flag, set the flag when the
 * CHIPSET_HARD_OFF hook is called when AP power state transist to G3 to
 * ensure the flag is set only when graceful shutdown.
 */
__override void pb_chipset_shutdown(void)
{
	return;
}

void board_set_ap_idle_at_g3()
{
	chip_save_reset_flags(chip_read_reset_flags() | EC_RESET_FLAG_AP_IDLE);
	system_set_reset_flags(EC_RESET_FLAG_AP_IDLE);
	CPRINTS("Saved AP_IDLE flag");
}
DECLARE_HOOK(HOOK_CHIPSET_HARD_OFF, board_set_ap_idle_at_g3,
	     HOOK_PRIO_PRE_DEFAULT);
#endif /* CONFIG_POWER_BUTTON_INIT_IDLE */
#endif /* CONFIG_X86_NON_DSX_PWRSEQ_MTL */
