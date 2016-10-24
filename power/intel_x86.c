/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Intel X86 chipset power control module for Chrome EC */

#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "host_command.h"
#include "intel_x86.h"
#include "lpc.h"
#include "power.h"
#include "power_button.h"
#include "system.h"
#include "task.h"
#include "util.h"
#include "wireless.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

#define CHARGER_INITIALIZED_DELAY_MS 100
#define CHARGER_INITIALIZED_TRIES 40

enum sys_sleep_state {
	SYS_SLEEP_S5,
	SYS_SLEEP_S4,
	SYS_SLEEP_S3
};

int power_s5_up;       /* Chipset is sequencing up or down */

#ifdef CONFIG_POWER_S0IX
static struct {
	int required; /* indicates de-bounce required. */
	int done;     /* debounced */
} slp_s0_debounce = {
	.required = 0,
	.done = 1,
};

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
#endif

/* Get system sleep state through GPIOs or VWs */
static int chipset_get_sleep_signal(enum sys_sleep_state state)
{
#ifdef CONFIG_VW_SIGNALS
	if (state == SYS_SLEEP_S4)
		return espi_vw_get_wire(VW_SLP_S4_L);
	else if (state == SYS_SLEEP_S3)
		return espi_vw_get_wire(VW_SLP_S3_L);
#else
	if (state == SYS_SLEEP_S4)
		return gpio_get_level(GPIO_PCH_SLP_S4_L);
	else if (state == SYS_SLEEP_S3)
		return gpio_get_level(GPIO_PCH_SLP_S3_L);
#endif

	/* We should never run here */
	ASSERT(0);
	return 0;
}

#ifdef CONFIG_BOARD_HAS_RTC_RESET
static enum power_state power_wait_s5_rtc_reset(void)
{
	static int s5_exit_tries;

	/* Wait for S5 exit and then attempt RTC reset */
	while ((power_get_signals() & IN_PCH_SLP_S4_DEASSERTED) == 0) {
#ifdef CONFIG_CHIPSET_SKYLAKE
		/* Handle RSMRST passthru event while waiting */
		handle_rsmrst(POWER_S5);
#endif

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

enum power_state _power_handle_state(enum power_state state)
{
	int tries = 0;

	switch (state) {
	case POWER_G3:
		break;

	case POWER_S5:
#ifdef CONFIG_CHIPSET_SKYLAKE
		if (forcing_shutdown) {
			power_button_pch_release();
			forcing_shutdown = 0;
		}
#endif

#ifdef CONFIG_BOARD_HAS_RTC_RESET
		/* Wait for S5 exit and attempt RTC reset it supported */
		if (power_s5_up)
			return power_wait_s5_rtc_reset();
#endif

#ifdef CONFIG_CHIPSET_APOLLOLAKE
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S5G3;
		}
#endif

		if (chipset_get_sleep_signal(SYS_SLEEP_S4) == 1)
			return POWER_S5S3; /* Power up to next state */
		break;

	case POWER_S3:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S3S5;
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1) {
			/* Power up to next state */
			return POWER_S3S0;
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S4) == 0) {
			/* Power down to next state */
			return POWER_S3S5;
		}
		break;

	case POWER_S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			chipset_force_shutdown();
			return POWER_S0S3;
#ifdef CONFIG_POWER_S0IX
		} else if ((gpio_get_level(GPIO_PCH_SLP_S0_L) == 0) &&
			   (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1)) {
			return POWER_S0S0ix;
#endif
		} else if (chipset_get_sleep_signal(SYS_SLEEP_S3) == 0) {
			/* Power down to next state */
			return POWER_S0S3;
		}

		break;

#ifdef CONFIG_POWER_S0IX
	case POWER_S0ix:
		/*
		 * TODO: add code for unexpected power loss
		 */
		if ((gpio_get_level(GPIO_PCH_SLP_S0_L) == 1) &&
		   (chipset_get_sleep_signal(SYS_SLEEP_S3) == 1)) {
			return POWER_S0ixS0;
		}

		break;
#endif

	case POWER_G3S5:
#ifdef CONFIG_CHIPSET_APOLLOLAKE
		/* Platform is powering up, clear forcing_coldreset */
		forcing_coldreset = 0;
#endif
		/*
		 * Allow up to 1s for charger to be initialized, in case
		 * we're trying to boot the AP with no battery.
		 */
		while (charge_prevent_power_on(0) &&
		       tries++ < CHARGER_INITIALIZED_TRIES) {
			msleep(CHARGER_INITIALIZED_DELAY_MS);
		}

		/* Return to G3 if battery level is too low */
		if (charge_want_shutdown() ||
		    tries > CHARGER_INITIALIZED_TRIES) {
			CPRINTS("power-up inhibited");
			chipset_force_shutdown();
			return POWER_G3;
		}

		/* Call hooks to initialize PMIC */
		hook_notify(HOOK_CHIPSET_PRE_INIT);

#ifdef CONFIG_CHIPSET_APOLLOLAKE
		/* Wait for RSMRST_L de-assert */
		if (power_wait_signals(IN_PGOOD_ALL_CORE)) {
#elif defined(CONFIG_CHIPSET_SKYLAKE)
		if (power_wait_signals(IN_PCH_SLP_SUS_DEASSERTED)) {
#endif
			chipset_force_shutdown();
			return POWER_G3;
		}

		power_s5_up = 1;
		return POWER_S5;

	case POWER_S5S3:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S5G3;
		}

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_STARTUP);
		return POWER_S3;

	case POWER_S3S0:
		if (!power_has_signals(IN_PGOOD_ALL_CORE)) {
			/* Required rail went away */
			chipset_force_shutdown();
			return POWER_S3S5;
		}

		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 1);

		/* Enable wireless */
		wireless_set_state(WIRELESS_ON);

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		/*
		 * Throttle CPU if necessary.  This should only be asserted
		 * when +VCCP is powered (it is by now).
		 */
		gpio_set_level(GPIO_CPU_PROCHOT, 0);

		return POWER_S0;

	case POWER_S0S3:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		gpio_set_level(GPIO_ENABLE_BACKLIGHT, 0);

		/* Suspend wireless */
		wireless_set_state(WIRELESS_SUSPEND);

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S3 or lower.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S3;

#ifdef CONFIG_POWER_S0IX
	case POWER_S0S0ix:
		/* call hooks before standby */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		lpc_enable_wake_mask_for_lid_open();

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S0ix.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S0ix;


	case POWER_S0ixS0:
		lpc_disable_wake_mask_for_lid_open();

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S0;
#endif

	case POWER_S3S5:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN);

		/* Disable wireless */
		wireless_set_state(WIRELESS_OFF);

		/* Always enter into S5 state. The S5 state is required to
		 * correctly handle global resets which have a bit of delay
		 * while the SLP_Sx_L signals are asserted then deasserted.
		 */
		power_s5_up = 0;
		return POWER_S5;

	case POWER_S5G3:
		chipset_force_g3();

#ifdef CONFIG_CHIPSET_APOLLOLAKE
		/* Power up the platform again for forced cold reset */
		if (forcing_coldreset) {
			forcing_coldreset = 0;
			return POWER_G3S5;
		}
#endif
		return POWER_G3;

	default:
		break;
	}

	return state;
}

#ifdef CONFIG_POWER_S0IX
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
#endif
