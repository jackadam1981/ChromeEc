/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Watchdog driver */

#include "common.h"
#include "gpio.h"
#include "hooks.h"
#include "hwtimer.h"
#include "registers.h"
#include "task.h"
#include "timer.h"
#include "util.h"
#include "watchdog.h"

/*
 * LSI oscillator frequency is typically 38 kHz, but it may be between 28-56
 * kHz and we don't calibrate it to know.  Use 56 kHz so that we pick a counter
 * value large enough that we reload before the worst-case watchdog delay
 * (fastest LSI clock).
 */
#ifdef CHIP_FAMILY_STM32L4
#define LSI_CLOCK 32000
#else
#define LSI_CLOCK 56000
#endif

#define HAL_IWDG_DEFAULT_TIMEOUT ((6UL * 256UL * 1000UL) / LSI_CLOCK)


/*
 * Use largest prescaler divider = /256.  This gives a worst-case watchdog
 * clock of 56000/256 = 218 Hz, and a maximum timeout period of (4095/218 Hz) =
 * 18.7 sec.
 *
 * For STM32L4, LSI is 32000. Watchdog clock is 32000 / 256 = 125Hz
 */
#define IWDG_PRESCALER 6
#define IWDG_PRESCALER_DIV (4 << IWDG_PRESCALER)

void watchdog_reload(void)
{
	/* Reload the watchdog */
	STM32_IWDG_KR = STM32_IWDG_KR_RELOAD;

#ifdef CONFIG_WATCHDOG_HELP
	hwtimer_reset_watchdog();
#endif
}
DECLARE_HOOK(HOOK_TICK, watchdog_reload, HOOK_PRIO_DEFAULT);

int watchdog_init(void)
{
#ifdef	CHIP_FAMILY_STM32L4
	timestamp_t tickstart, ticknow;

	/* Enable watchdog registers */
	STM32_IWDG_KR = STM32_IWDG_KR_START;

	/*	Enable access to IWDG	*/
	STM32_IWDG_KR = STM32_IWDG_KR_UNLOCK;

	/* Set the prescaler between the LSI clock and the watchdog counter */
	STM32_IWDG_PR = IWDG_PRESCALER & 7;

	/* Set the reload value of the watchdog counter */
	STM32_IWDG_RLR = MIN(STM32_IWDG_RLR_MAX, CONFIG_WATCHDOG_PERIOD_MS *
			     (LSI_CLOCK / IWDG_PRESCALER_DIV) / 1000);

	tickstart = get_time();
	/*	Wait for SR */
	while (STM32_IWDG_SR != 0x00u)	{
		ticknow = get_time();
		if ((ticknow.val - tickstart.val) >
		    HAL_IWDG_DEFAULT_TIMEOUT * 1000) {
			return EC_ERROR_TIMEOUT;
		}
	}

	/* Reload the watchdog */
	  STM32_IWDG_KR = STM32_IWDG_KR_RELOAD;

#else
	/* Unlock watchdog registers */
	STM32_IWDG_KR = STM32_IWDG_KR_UNLOCK;

	/* Set the prescaler between the LSI clock and the watchdog counter */
	STM32_IWDG_PR = IWDG_PRESCALER & 7;

	/* Set the reload value of the watchdog counter */
	STM32_IWDG_RLR = MIN(STM32_IWDG_RLR_MAX, CONFIG_WATCHDOG_PERIOD_MS *
			     (LSI_CLOCK / IWDG_PRESCALER_DIV) / 1000);

	/* Start the watchdog (and re-lock registers) */
	STM32_IWDG_KR = STM32_IWDG_KR_START;
#endif
#ifdef CONFIG_WATCHDOG_HELP
	/* Use a harder timer to warn about an impending watchdog reset */
	hwtimer_setup_watchdog();
#endif

	return EC_SUCCESS;
}
