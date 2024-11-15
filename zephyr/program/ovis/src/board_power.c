/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "console.h"
#include "gpio/gpio.h"
#include "gpio_signal.h"
#include "system.h"
#include "system_boot_time.h"
#include "zephyr_adc.h"

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
#define MINIMUM_POWER_IN_MV 1800
/*
 * The AP_IDLE flag is not expected to be set when power failure
 * (e.g. disconnect AC power). The flag is set when CHIPSET_SHUTDOWN
 * hook is called during AP power state S4 to S5 transition. On Deku,
 * the voltage drops slowly when AC power is disconnected so taht
 * PWR_RSMRST_PWRGD is still high when the chipset shutdown hook
 * function is called. In this case, the AP_IDLE flag is set unexpectly.
 * To address the issue, use the ADC to read the voltage of system
 * power, consider it is power fail when the voltage is lower than
 * certain level and bypass AP_IDLE flag.
 */
__override void pb_chipset_shutdown(void)
{
	if(adc_read_channel(ADC_PSYS) < MINIMUM_POWER_IN_MV) {
		CPRINTS("Voltage of PPVAR_SYS is too low, power failure!");
		return;
	}

	chip_save_reset_flags(chip_read_reset_flags() | EC_RESET_FLAG_AP_IDLE);
	system_set_reset_flags(EC_RESET_FLAG_AP_IDLE);
	CPRINTS("Voltage of PPVAR_SYS is good");
	CPRINTS("Saved AP_IDLE flag");

	return;
}

#endif /* CONFIG_POWER_BUTTON_INIT_IDLE */
#endif /* CONFIG_X86_NON_DSX_PWRSEQ_MTL */
