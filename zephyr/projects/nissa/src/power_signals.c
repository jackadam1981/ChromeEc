/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 *  * Use of this source code is governed by a BSD-style license that can be
 *   * found in the LICENSE file.
 *    */

#include "gpio.h"
#include "hooks.h"
#include "adc.h"
#include "power.h"
#include "power/icelake.h"
#include "power/intel_x86.h"

const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
		.delay_ms = 2,
	},
	{
		.gpio = GPIO_PCH_PWROK,
	},
	{
		.gpio = GPIO_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	/* No delays needed during S0 exit */
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
	},
	{
		.gpio = GPIO_PCH_PWROK,
	},
	{
		.gpio = GPIO_PCH_SYS_PWROK,
	},
	/* Turn off the VCCIN rail last */
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

__override void board_after_rsmrst(int rsmrst)
{
	/*
	 * b:148688874: If RSMRST# is de-asserted, enable the pull-up on
	 * PG_PP1050_ST_OD.  It won't be enabled prior to this signal going high
	 * because the load switch for PP1050_ST cannot pull the PG low.  Once
	 * it's asserted, disable the pull up so we don't inidicate that the
	 * power is good before the rail is actually ready.
	 */
	int flags = rsmrst ? GPIO_PULL_UP : 0;

	flags |= GPIO_INT_BOTH;

	gpio_set_flags(GPIO_PG_PP1050_PROC, flags);
}

