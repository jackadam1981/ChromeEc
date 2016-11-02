/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */

#include "apollolake.h"
#include "chipset.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "intel_x86.h"
#include "lpc.h"
#include "skylake.h"
#include "system.h"
#include "task.h"
#include "util.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#ifdef CONFIG_BOARD_HAS_RTC_RESET
enum power_state power_wait_s5_rtc_reset(void)
{
	static int s5_exit_tries;

	/* Wait for S5 exit and then attempt RTC reset */
	while ((power_get_signals() & IN_PCH_SLP_S4_DEASSERTED) == 0) {
		/* Handle RSMRST passthru event while waiting */
		handle_rsmrst(POWER_S5);
		if (task_wait_event(SECOND*4) == TASK_EVENT_TIMER) {
			CPRINTS("timeout waiting for S5 exit");
			chipset_force_g3();

			/* Assert RTCRST# and retry 5 times */
			board_rtc_reset();

			if (++s5_exit_tries > 4) {
				s5_exit_tries = 0;
				return POWER_G3; /* Stay off */
			}

			udelay(10 * MSEC);
			return POWER_G3S5; /* Power up again */
		}
	}

	s5_exit_tries = 0;
	return POWER_S5S3; /* Power up to next state */
}
#endif

void chipset_throttle_cpu(int throttle)
{
	if (chipset_in_state(CHIPSET_STATE_ON))
		gpio_set_level(GPIO_CPU_PROCHOT, throttle);
}

enum power_state power_chipset_init(void)
{
	/*
	 * If we're switching between images without rebooting, see if the x86
	 * is already powered on; if so, leave it there instead of cycling
	 * through G3.
	 */
	if (system_jumped_to_this_image()) {
		if ((power_get_signals() & IN_ALL_S0) == IN_ALL_S0) {
			/* Disable idle task deep sleep when in S0. */
			disable_sleep(SLEEP_MASK_AP_RUN);
			CPRINTS("already in S0");
			return POWER_S0;
		}

		/* Force all signals to their G3 states */
		chipset_force_g3();
	}

	return POWER_G3;
}

#ifdef CONFIG_POWER_S0IX
static struct {
	int required; /* indicates de-bounce required. */
	int done;     /* debounced */
} slp_s0_debounce = {
	.required = 0,
	.done = 1,
};

int chipset_get_ps_debounced_level(enum gpio_signal signal)
{
	/*
	 * If power state is updated in power_update_signal() by any interrupts
	 * other than SLP_S0 during the 1 msec pulse(invalid SLP_S0 signal),
	 * reading SLP_S0 should be corrected with slp_s0_debounce.done flag.
	 */
	int level = gpio_get_level(signal);

	return (signal == GPIO_PCH_SLP_S0_L) ?
			(level & slp_s0_debounce.done) : level;
}

static void slp_s0_assertion_deferred(void)
{
	int s0_level = gpio_get_level(GPIO_PCH_SLP_S0_L);

	if (s0_level == slp_s0_debounce.required) {
		if (s0_level)
			slp_s0_debounce.done = 1; /* debounced! */

		power_signal_interrupt(GPIO_PCH_SLP_S0_L);
	}

	slp_s0_debounce.required = 0;
}
DECLARE_DEFERRED(slp_s0_assertion_deferred);

void power_signal_interrupt_S0(enum gpio_signal signal)
{
	if (gpio_get_level(GPIO_PCH_SLP_S0_L)) {
		slp_s0_debounce.required = 1;
		hook_call_deferred(&slp_s0_assertion_deferred_data, 3 * MSEC);
	} else if (slp_s0_debounce.required == 0) {
		slp_s0_debounce.done = 0;
		slp_s0_assertion_deferred();
	}
}

/*
 * In AP S0 -> S3 & S0ix transitions,
 * the chipset_suspend is called.
 *
 * The chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)
 * is used to detect the S0ix transiton.
 *
 * During S0ix entry, the wake mask for lid open is enabled.
 */
void lpc_enable_wake_mask_for_lid_open(void)
{
	if ((chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)) ||
				chipset_in_state(CHIPSET_STATE_STANDBY)) {
		uint32_t mask = 0;

		mask = ((lpc_get_host_event_mask(LPC_HOST_EVENT_WAKE)) |
			EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_OPEN));

		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, mask);
}	}

/*
 * In AP S0ix & S3 -> S0 transitions,
 * the chipset_resume hook is called.
 *
 * During S0ix exit, the wake mask for lid open is disabled.
 * All pending events are cleared
 */
void lpc_disable_wake_mask_for_lid_open(void)
{
	if ((chipset_in_state(CHIPSET_STATE_STANDBY | CHIPSET_STATE_ON)) ||
				chipset_in_state(CHIPSET_STATE_ON)) {
		lpc_set_host_event_mask(LPC_HOST_EVENT_WAKE, 0);

		/* Clear host events */
		while (lpc_query_host_event_state())
			;
	}
}
#endif
