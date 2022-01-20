/* Copyright 2017 The Chromium OS Authors. All rights reserved.
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

/* AMD x86 power sequencing module for Chrome EC */


#include "lid_switch.h"
#include "power/common_x86.h"
#include "power/amd_x86.h"
#include "registers.h"
#include "timer.h"
#include "usb_charge.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ##args)

const int sleep_sig[] = {
	[SYS_SLEEP_S3] = GPIO_PCH_SLP_S3_L,
	[SYS_SLEEP_S5] = GPIO_PCH_SLP_S5_L,
#ifdef CONFIG_POWER_S0IX
	[SYS_SLEEP_S0IX] = GPIO_PCH_SLP_S0_L,
#endif
};

#define IN_S5_PGOOD POWER_SIGNAL_MASK(X86_S5_PGOOD)

static int forcing_shutdown; /* Forced shutdown in progress? */

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
	if ((power_get_signals() & IN_S5_PGOOD) && forcing_shutdown) {
		power_button_pch_release();
		forcing_shutdown = 0;
	}
}

enum power_state power_chipset_init(void)
{
	CPRINTS("%s: power_signal=0x%x", __func__, power_get_signals());

	if (!system_jumped_to_this_image())
		return POWER_G3;
	/*
	 * We are here as RW. We need to handle the following cases:
	 *
	 * 1. Late sysjump by software sync. AP is in S0.
	 * 2. Shutting down in recovery mode then sysjump by EFS2. AP is in S5
	 *    and expected to sequence down.
	 * 3. Rebooting from recovery mode then sysjump by EFS2. AP is in S5
	 *    and expected to sequence up.
	 * 4. RO jumps to RW from main() by EFS2. (a.k.a. power on reset, cold
	 *    reset). AP is in G3.
	 */
	if (gpio_get_level(GPIO_S0_PGOOD)) {
		/* case #1. Disable idle task deep sleep when in S0. */
		disable_sleep(SLEEP_MASK_AP_RUN);
		CPRINTS("already in S0");
		return POWER_S0;
	}
	if (power_get_signals() & IN_S5_PGOOD) {
		/* case #2 & #3 */
		CPRINTS("already in S5");
		return POWER_S5;
	}
	/* case #4 */
	chipset_force_g3();
	return POWER_G3;
}

static void handle_pass_through(enum gpio_signal pin_in,
				enum gpio_signal pin_out)
{
	/*
	 * Pass through asynchronously, as SOC may not react
	 * immediately to power changes.
	 */
	int in_level = gpio_get_level(pin_in);
	int out_level = gpio_get_level(pin_out);

	/*
	 * Only pass through high S0_PGOOD (S0 power) when S5_PGOOD (S5 power)
	 * is also high (S0_PGOOD is pulled high in G3 when S5_PGOOD is low).
	 */
	if ((pin_in == GPIO_S0_PGOOD) && !gpio_get_level(GPIO_S5_PGOOD))
		in_level = 0;

	/* Nothing to do. */
	if (in_level == out_level)
		return;

	/*
	 * SOC requires a delay of 1ms with stable power before
	 * asserting PWR_GOOD.
	 */
	if ((pin_in == GPIO_S0_PGOOD) && in_level)
		msleep(1);

	if (IS_ENABLED(CONFIG_CHIPSET_X86_RSMRST_DELAY) &&
	    (pin_out == GPIO_PCH_RSMRST_L) && in_level)
		msleep(10);

	gpio_set_level(pin_out, in_level);

	CPRINTS("Pass through %s: %d", gpio_get_name(pin_in), in_level);
}

enum power_state power_handle_state(enum power_state state)
{
	handle_pass_through(GPIO_S5_PGOOD, GPIO_PCH_RSMRST_L);

	handle_pass_through(GPIO_S0_PGOOD, GPIO_PCH_SYS_PWROK);

	if (state == POWER_S5 && forcing_shutdown) {
		power_button_pch_release();
		forcing_shutdown = 0;
	}

	switch (state) {
	case POWER_G3:
		break;

	case POWER_G3S5:
		/* Exit SOC G3 */
		/* Enable system power ("*_A" rails) in S5. */
		gpio_set_level(GPIO_EN_PWR_A, 1);

		/*
		 * Callback to do pre-initialization within the context of
		 * chipset task.
		 */
		if (IS_ENABLED(CONFIG_CHIPSET_HAS_PRE_INIT_CALLBACK))
			chipset_pre_init_callback();

		if (power_wait_signals(IN_S5_PGOOD)) {
			chipset_force_g3();
			return POWER_G3;
		}

		CPRINTS("Exit SOC G3");

		return POWER_S5;

	case POWER_S5:
		if (!power_has_signals(IN_S5_PGOOD)) {
			/* Required rail went away */
			return POWER_S5G3;
		} else if (gpio_get_level(GPIO_PCH_SLP_S5_L) == 1) {
			/* Power up to next state */
			return POWER_S5S3;
		}
		break;

	case POWER_S5S3:
		if (!power_has_signals(IN_S5_PGOOD)) {
			/* Required rail went away */
			return POWER_S5G3;
		}

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_STARTUP);

#ifdef CONFIG_POWER_S0IX
		/*
		 * Clearing the S0ix flag on the path to S0
		 * to handle any reset conditions.
		 */
		power_reset_host_sleep_state();
#endif
		return POWER_S3;

	case POWER_S3:
		if (!power_has_signals(IN_S5_PGOOD)) {
			/* Required rail went away */
			return POWER_S5G3;
		} else if (gpio_get_level(GPIO_PCH_SLP_S3_L) == 1) {
			/* Power up to next state */
			return POWER_S3S0;
		} else if (gpio_get_level(GPIO_PCH_SLP_S5_L) == 0) {
			/* Power down to next state */
			return POWER_S3S5;
		}
		break;

	case POWER_S3S0:
		if (!power_has_signals(IN_S5_PGOOD)) {
			/* Required rail went away */
			return POWER_S5G3;
		}

		/* Enable wireless */
		wireless_set_state(WIRELESS_ON);

		lpc_s3_resume_clear_masks();

		/* Call hooks now that rails are up */
		hook_notify(HOOK_CHIPSET_RESUME);

		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		return POWER_S0;

	case POWER_S0:
		if (!power_has_signals(IN_S5_PGOOD)) {
			/* Required rail went away */
			return POWER_S5G3;
		}
#ifdef CONFIG_POWER_S0IX
		/*
		 * SLP_S0 may assert in system idle scenario without a kernel
		 * freeze call. This may cause interrupt storm since there is
		 * no freeze/unfreeze of threads/process in the idle scenario.
		 * Ignore the SLP_S0 assertions in idle scenario by checking
		 * the host sleep state.
		 */
		else if (power_get_host_sleep_state()
					== HOST_SLEEP_EVENT_S0IX_SUSPEND &&
				gpio_get_level(GPIO_PCH_SLP_S0_L) == 0) {
			return POWER_S0S0ix;
		}
#endif
		else if (gpio_get_level(GPIO_PCH_SLP_S3_L) == 0) {
			/* Power down to next state */
			return POWER_S0S3;
		}
#ifdef CONFIG_POWER_S0IX
		/*
		 * Call hooks only if we haven't notified listeners of S0ix
		 * resume.
		 */
		sleep_notify_transition(SLEEP_NOTIFY_RESUME,
					HOOK_CHIPSET_RESUME);
#endif
		break;

	case POWER_S0S3:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SUSPEND);

		/* Suspend wireless */
		wireless_set_state(WIRELESS_SUSPEND);

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S3 or lower.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);

#ifdef CONFIG_POWER_S0IX
		/* re-init S0ix flag */
		power_reset_host_sleep_state();
#endif
		return POWER_S3;

	case POWER_S3S5:
		/* Call hooks before we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN);

		/* Disable wireless */
		wireless_set_state(WIRELESS_OFF);

		/* Call hooks after we remove power rails */
		hook_notify(HOOK_CHIPSET_SHUTDOWN_COMPLETE);

		return POWER_S5;

	case POWER_S5G3:
		chipset_force_g3();

		return POWER_G3;

#ifdef CONFIG_POWER_S0IX
	case POWER_S0ix:
		/* System in S0 only if SLP_S0 and SLP_S3 are de-asserted */
		if ((gpio_get_level(GPIO_PCH_SLP_S0_L) == 1) &&
		    (gpio_get_level(GPIO_PCH_SLP_S3_L) == 1)) {
			return POWER_S0ixS0;
		} else if (!power_has_signals(IN_S5_PGOOD)) {
			/* Lost power, start transition to G3 */
			return POWER_S0;
		}

		break;

	case POWER_S0S0ix:
		/*
		 * Call hooks only if we haven't notified listeners of S0ix
		 * suspend.
		 */
		sleep_notify_transition(SLEEP_NOTIFY_SUSPEND,
					HOOK_CHIPSET_SUSPEND);
		sleep_suspend_transition();

		/*
		 * Enable idle task deep sleep. Allow the low power idle task
		 * to go into deep sleep in S0ix.
		 */
		enable_sleep(SLEEP_MASK_AP_RUN);
		return POWER_S0ix;

	case POWER_S0ixS0:
		/*
		 * Disable idle task deep sleep. This means that the low
		 * power idle task will not go into deep sleep while in S0.
		 */
		disable_sleep(SLEEP_MASK_AP_RUN);

		sleep_resume_transition();
		return POWER_S0;
#endif /* CONFIG_POWER_S0IX */
	default:
		break;
	}
	return state;
}
