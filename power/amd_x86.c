/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AMD x86 power sequencing module for Chrome EC */

#include "power/common_x86.h"
#include "power/amd_x86.h"
#include "registers.h"
#include "timer.h"
#include "usb_charge.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ##args)

static int forcing_shutdown; /* Forced shutdown in progress? */

const int sleep_sig[] = {
	[SYS_SLEEP_S3] = GPIO_PCH_SLP_S3_L,
	[SYS_SLEEP_S5] = GPIO_PCH_SLP_S5_L,
#ifdef CONFIG_POWER_S0IX
	[SYS_SLEEP_S0IX] = GPIO_PCH_SLP_S0_L,
#endif
};

const int sleep_next_state_pf[] = {
	[POWER_S0]	= POWER_S5S3,
	[POWER_S3]	= POWER_S3S5,
	[POWER_S3S0]	= POWER_S3S5,
	[POWER_S5]	= POWER_S5G3,
	[POWER_S5S3]	= POWER_S5G3,
	[POWER_G3S5]	= POWER_G3,
};

const int sleep_next_state_up[] = {
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S0,
	[POWER_S0ixS0]	= POWER_S0,
#endif
	[POWER_S3]	= POWER_S0,
	[POWER_S3S0]	= POWER_S0,
	[POWER_S5]	= POWER_S3,
	[POWER_S5S3]	= POWER_S3,
	[POWER_G3S5]	= POWER_S5,
};

const int sleep_next_transition_up[] = {
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S0ixS0,
#endif
	[POWER_S3]	= POWER_S3S0,
	[POWER_S5]	= POWER_S5S3,
};

const int sleep_next_state_down[] = {
	[POWER_S0]	= POWER_S3,
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S3,
	[POWER_S0S0ix]	= POWER_S0ix,
#endif
	[POWER_S0S3]	= POWER_S3,
	[POWER_S3]	= POWER_S5,
	[POWER_S3S5]	= POWER_S5,
	[POWER_G3S5]	= POWER_G3,
};

const int sleep_next_transition_down[] = {
	[POWER_S0]	= POWER_S0S3,
	[POWER_S3]	= POWER_S3S5,
};

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	CPRINTS("%s()", __func__);
	switch (reason) {
	case CHIPSET_SHUTDOWN_G3:
		/* Disable system power ("*_A" rails) in G3. */
		gpio_set_level(GPIO_EN_PWR_A, 0);
		break;
	default:
		if (!chipset_in_or_transitioning_to_state(
				CHIPSET_STATE_ANY_OFF)) {
			forcing_shutdown = 1;
			power_button_pch_press();
			report_ap_reset(reason);
		}
		break;
	}
}

void chipset_handle_espi_reset_assert(void)
{
	/*
	 * eSPI_Reset# pin being asserted without RSMRST# being asserted
	 * means there is an unexpected power loss (global reset event).
	 * In this case, check if the shutdown is forced by the EC (due
	 * to battery, thermal, or console command). The forced shutdown
	 * initiates a power button press that we need to release.
	 *
	 * NOTE: S5_PGOOD input is passed through to the RSMRST# output to
	 * the AP.
	 */
	if ((power_get_signals() & CHIPSET_G3S5_POWERUP_SIGNAL) &&
		forcing_shutdown) {
		power_button_pch_release();
		forcing_shutdown = 0;
	}
}

__override bool is_passthrough_valid(enum gpio_signal pin_in,
		enum gpio_signal pin_out, int *p_in_level)
{

	/*
	 * Only pass through high S0_PGOOD (S0 power) when S5_PGOOD (S5 power)
	 * is also high (S0_PGOOD is pulled high in G3 when S5_PGOOD is low).
	 */
	if ((pin_in == GPIO_S0_PGOOD) && !gpio_get_level(GPIO_S5_PGOOD))
		*p_in_level = 0;

	/*
	 * SOC requires a delay of 1ms with stable power before
	 * asserting PWR_GOOD.
	 */
	if ((pin_in == GPIO_S0_PGOOD) && *p_in_level)
		msleep(1);

	if (IS_ENABLED(CONFIG_CHIPSET_X86_RSMRST_DELAY) &&
	    (pin_out == GPIO_PCH_RSMRST_L) && *p_in_level)
		msleep(10);

	return true;
}

enum power_state power_handle_state(enum power_state state)
{
	handle_pass_through(GPIO_S5_PGOOD, GPIO_PCH_RSMRST_L);

	handle_pass_through(GPIO_S0_PGOOD, GPIO_PCH_SYS_PWROK);

	if (state == POWER_S5 && forcing_shutdown) {
		power_button_pch_release();
		forcing_shutdown = 0;
	}

	return common_x86_power_handle_state(state);

}
