/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "power/meteorlake.h"

/******************************************************************************/
#if 0
/* PWROK signal configuration */
/*
 * On MTLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);
#endif

void board_ap_power_action_g3_s5(void)
{
	LOG_DBG("Turning on PWR_EN_DS3");
	power_signal_set(PWR_EN_DS3, 1);

	power_wait_signals_timeout(IN_PGOOD_ALL_CORE,
		AP_PWRSEQ_DT_VALUE(wait_signal_timeout));
}

int board_ap_power_assert_pch_power_ok(void)
{
	return 0;
}
