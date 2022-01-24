/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */

#include "power/common_x86.h"
#include "power/intel_x86.h"

#include "power.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ##args)

const int sleep_sig[] = {
	[SYS_SLEEP_S3] = SLP_S3_SIGNAL_L,
	[SYS_SLEEP_S4] = SLP_S4_SIGNAL_L,
	[SYS_SLEEP_S5] = SLP_S5_SIGNAL_L,
#ifdef CONFIG_POWER_S0IX
	[SYS_SLEEP_S0IX] = GPIO_PCH_SLP_S0_L,
#endif
};

const int sleep_next_state_pf[] = {
	[POWER_S0]	= POWER_S0S3,
	[POWER_S3]	= POWER_S3S5,
	[POWER_S3S0]	= POWER_S3S5,
	[POWER_S5]	= POWER_S5G3,
	[POWER_S5S3]	= POWER_S5S3,
	[POWER_G3S5]	= POWER_G3,
};

const int sleep_next_state_up[] = {
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S0,
	[POWER_S0ixS0]	= POWER_S0,
#endif
	[POWER_S3]	= POWER_S0,
	[POWER_S3S0]	= POWER_S0,
	[POWER_S4]	= POWER_S3,
	[POWER_S5]	= POWER_S4,
	[POWER_S5S3]	= POWER_S3,
	[POWER_S5S4]	= POWER_S4,
	[POWER_G3S5]	= POWER_S5,
};

const int sleep_next_transition_up[] = {
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S0ixS0,
#endif
	[POWER_S3]	= POWER_S3S0,
	[POWER_S4]	= POWER_S4S3,
	[POWER_S5]	= POWER_S5S4,
};

const int sleep_next_state_down[] = {
	[POWER_S0]	= POWER_S3,
#ifdef CONFIG_POWER_S0IX
	[POWER_S0ix]	= POWER_S3,
	[POWER_S0S0ix]	= POWER_S0ix,
#endif
	[POWER_S0S3]	= POWER_S3,
	[POWER_S3]	= POWER_S4,
	[POWER_S3S4]	= POWER_S4,
	[POWER_S3S5]	= POWER_S5,
	[POWER_S4]	= POWER_S5,
	[POWER_S4S5]	= POWER_S5,
	[POWER_G3S5]	= POWER_G3,
};

const int sleep_next_transition_down[] = {
	[POWER_S0]	= POWER_S0S3,
	[POWER_S3]	= POWER_S3S4,
	[POWER_S4]	= POWER_S4S5,
};

#ifdef CONFIG_CHARGER
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, power_up_inhibited_cb, HOOK_PRIO_DEFAULT);
#endif

#ifdef CONFIG_BOARD_HAS_RTC_RESET
static void intel_x86_rtc_reset(void)
{
	CPRINTS("Asserting RTCRST# to PCH");
	gpio_set_level(GPIO_PCH_RTCRST, 1);
	udelay(100);
	gpio_set_level(GPIO_PCH_RTCRST, 0);
}

static enum power_state power_wait_s5_rtc_reset(void)
{
	static int s5_exit_tries;

	/* Wait for S5 exit and then attempt RTC reset */
	while ((power_get_signals() & IN_PCH_SLP_S4_DEASSERTED) == 0) {
		/* Handle RSMRST passthru event while waiting */
		common_intel_x86_handle_rsmrst(POWER_S5);
		if (task_wait_event(SECOND*4) == TASK_EVENT_TIMER) {
			CPRINTS("timeout waiting for S5 exit");
			chipset_force_g3();

			/* Assert RTCRST# and retry 5 times */
			intel_x86_rtc_reset();

			if (++s5_exit_tries > 4) {
				s5_exit_tries = 0;
				return POWER_G3; /* Stay off */
			}

			udelay(10 * MSEC);
			return POWER_G3S5; /* Power up again */
		}
	}

	s5_exit_tries = 0;
	return POWER_S5S4; /* Power up to next state */
}

__overridable enum power_state power_wait_rtc_reset(enum power_state state)
{
	switch (state) {
	case POWER_S5:
		return power_wait_s5_rtc_reset();
		break;
	default:
		return state;
	}
}
#endif

void common_intel_x86_handle_rsmrst(enum power_state state)
{
	handle_pass_through_with_callbacks(GPIO_PG_EC_RSMRST_ODL,
		GPIO_PCH_RSMRST_L, &board_before_rsmrst, &board_after_rsmrst);
}
