/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sys/atomic.h>
#include <x86_non_dsx_adlp_pwrseq_sm.h>
#include <x86_non_dsx_common_pwrseq_sm_handler.h>
#include "drivers/sensor.h"

LOG_MODULE_REGISTER(common_pwrseq, LOG_LEVEL_INF);

#define  X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS	5

static void generate_ec_soc_dsw_pwrok_handler(const struct common_pwrseq_config
				       *com_cfg)
{
	int in_sig_val =  power_signal_is_asserted(X86_DSW_PWROK);

	if (in_sig_val != gpio_pin_get_dt(&com_cfg->ec_soc_dsw_pwrok)) {
		if (in_sig_val)
			k_msleep(com_cfg->pch_dsw_pwrok_delay_ms);
		gpio_pin_set_dt(&com_cfg->ec_soc_dsw_pwrok, 1);
	}
}

/* Override */
void chipset_force_shutdown(enum pwrseq_chipset_shutdown_reason reason,
			    const struct common_pwrseq_config *com_cfg)
{
	int timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;

	gpio_pin_set_dt(&com_cfg->ec_pch_rsmrst_odl, 0);
	gpio_pin_set_dt(&com_cfg->ec_soc_dsw_pwrok, 0);

	while (gpio_pin_get_dt(&com_cfg->pg_ec_rsmrst_odl) == 1 &&
	      gpio_pin_get_dt(&com_cfg->slp_sus_l) == 1 && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	}
	if (gpio_pin_get_dt(&com_cfg->slp_sus_l) == 1) {
		LOG_WRN("SLP_SUS is not deasserted! Assuming G3");
	}

	if (gpio_pin_get_dt(&com_cfg->pg_ec_rsmrst_odl) == 1) {
		LOG_WRN("RSMRST is not deasserted! Assuming G3");
	}

	gpio_pin_set_dt(&com_cfg->enable_pp3300_a, 0);

	gpio_pin_set_dt(&com_cfg->enable_pp5000_a, 0);

	timeout_ms = X86_NON_DSX_ADLP_NONPWRSEQ_FORCE_SHUTDOWN_TO_MS;
	while (power_signal_is_asserted(X86_DSW_PWROK) && timeout_ms > 0) {
		k_msleep(1);
		timeout_ms--;
	};

	if (power_signal_is_asserted(X86_DSW_PWROK))
		LOG_WRN("DSW_PWROK didn't go low!  Assuming G3.");
}

/* Override */
void g3s5_action_handler(const struct common_pwrseq_config *com_cfg)
{
	gpio_pin_set_dt(&com_cfg->enable_pp5000_a, 1);

	gpio_pin_set_dt(&com_cfg->enable_pp3300_a, 1);

	power_wait_signals(IN_PGOOD_ALL_CORE);

	generate_ec_soc_dsw_pwrok_handler(com_cfg);
}

/* Override */
int generate_pch_pwrok_handler(const struct chipset_pwrseq_config *chip_cfg)
{
	/* Pass though PCH_PWROK */
	if (gpio_pin_get_dt(&chip_cfg->pch_pwrok) == 0) {
		k_msleep(chip_cfg->pch_pwrok_delay_ms);
		gpio_pin_set_dt(&chip_cfg->pch_pwrok, 1);
		LOG_DBG("Set PCH_PWROK\n");
	}

	return 0;
}

/* Override */
int board_power_signal_is_asserted(enum board_power_signal signal)
{
	if (signal != BOARD_ALL_SYS_PGOOD) {
		return 0;
	}

	if (power_has_signals(IN_PCH_SLP_S3_DEASSERTED) == 0) {
		LOG_WRN("SLP_S3 is 0");
		return 0;
	}
	if (power_signal_is_asserted(GPIO_ALL_SYS_PGOOD) == 0) {
		LOG_WRN("GPIO_ALL_SYS_PGOOD is 0");
		return 0;
	}
	if (pwrseq_adc_get_level(VSENSE_PP1P05) == 0) {
		LOG_WRN("PP1050_PROC is 0");
		return 0;
	}
	return 1;
}
