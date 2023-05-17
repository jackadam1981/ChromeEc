/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel MTL-P-RVP board-specific configuration */

#include "console.h"
#include "gpio.h"
#include "lid_switch.h"
#include "power.h"
#include "power/meteorlake.h"
#include "power_button.h"
#include "registers.h"

/* PWROK signal configuration */
/*
 * On MTLRVP, SYS_PWROK_EC is an output controlled by EC and uses ALL_SYS_PWRGD
 * as input.
 */
const struct intel_x86_pwrok_signal pwrok_signal_assert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
		.delay_ms = 3,
	},
};
const int pwrok_signal_assert_count = ARRAY_SIZE(pwrok_signal_assert_list);

const struct intel_x86_pwrok_signal pwrok_signal_deassert_list[] = {
	{
		.gpio = GPIO_SYS_PWROK_EC,
	},
};
const int pwrok_signal_deassert_count = ARRAY_SIZE(pwrok_signal_deassert_list);

int extpower_is_present(void)
{
	return gpio_get_level(GPIO_BC_ACOK_EC);
}

__override int board_get_version(void)
{
	return 1;
}

static void fake_interrupt(enum gpio_signal signal)
{
}

#include "gpio_list.h"
/******************************************************************************/
