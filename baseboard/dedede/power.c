/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gpio.h"
#include "hooks.h"
#include "power/icelake.h"

const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
		.delay_ms = 2,
	},
	{
		.gpio = GPIO_PCH_DSW_PWROK,
	},
	{
		.gpio = GPIO_EC_PCH_SYS_PWROK,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	/* No delays needed during S0 exit */
	{
		.gpio = GPIO_VCCST_PWRGD_OD,
	},
	{
		.gpio = GPIO_PCH_DSW_PWROK,
	},
	{
		.gpio = GPIO_EC_PCH_SYS_PWROK,
	},
	/* Turn off the VCCIN rail last */
	{
		.gpio = GPIO_PG_EC_ALL_SYS_PWRGD,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

int extpower_is_present(void)
{
	/* Empty function to satisfy compiler and avoid build failure */
	return 0;
}
