/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Icelake chipset power control module for Chrome EC */

#include "cometlake.h"
#include "chipset.h"
#include "console.h"
#include "gpio.h"
#include "intel_x86.h"
#include "power.h"
#include "power_button.h"
#include "task.h"
#include "timer.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)

/* The wait time is ~150 msec, allow for safety margin. */
#define IN_PGOOD_ALL_CORE_WAIT_TIME_USEC	(250 * MSEC)

static int forcing_shutdown;  /* Forced shutdown in progress? */

void chipset_force_shutdown(enum chipset_shutdown_reason reason)
{
	int timeout_ms = 50;

	CPRINTS("%s()", __func__, reason);
	report_ap_reset(reason);

	/* Turn off RMSRST_L to PCH */
	gpio_set_level(GPIO_EC_PCH_RSMRST_L, 0);

	/* Turn off A (except PP5000_A) rails*/
	gpio_set_level(GPIO_EN_A_RAILS, 0);

	/* Turn off PP5000_A rail */
	gpio_set_level(GPIO_EN_PP5000_A, 0);

	/*
	 * TODO(b/111810925): Replace this wait with
	 * power_wait_signals_timeout()
	 */
	/* Now wait for PP5000_A rail to go away */
	while (gpio_get_level(GPIO_PP5000_A_PG_OD) && (timeout_ms > 0)) {
		msleep(1);
		timeout_ms--;
	};

	if (!timeout_ms)
		CPRINTS("PP5000_A rail still up!  Assuming G3.");
}

void chipset_handle_espi_reset_assert(void)
{
	/*
	 * If eSPI_Reset# pin is asserted without SLP_SUS# being asserted, then
	 * it means that there is an unexpected power loss (global reset
	 * event). In this case, check if shutdown was being forced by pressing
	 * power button. If yes, release power button.
	 */
	if ((power_get_signals() & IN_PGOOD_ALL_CORE) &&
		forcing_shutdown) {
		power_button_pch_release();
		forcing_shutdown = 0;
	}
}

enum power_state chipset_force_g3(void)
{
	chipset_force_shutdown(CHIPSET_SHUTDOWN_G3);

	return POWER_G3;
}

enum power_state power_handle_state(enum power_state state)
{

	common_intel_x86_handle_rsmrst(state);

	switch (state) {

	case POWER_G3S5:

		/*
		 * Wait for RSMRST from power good chip. If this signal doesn't
		 * go high within 250 msec, then go back to G3.
		 */
		if (power_wait_signals_timeout(IN_PGOOD_ALL_CORE,
					       IN_PGOOD_ALL_CORE_WAIT_TIME_USEC)
						!= EC_SUCCESS) {
			CPRINTS("RSMRST_L didn't go high!  Assuming G3.");
			return POWER_G3;
		}

		/* Wait 10 msec before passing this signal indication to AP */
		msleep(10);
		gpio_set_level(GPIO_EC_PCH_RSMRST_L, 1);
		break;

	case POWER_S5:
		if (forcing_shutdown) {
			power_button_pch_release();
			forcing_shutdown = 0;
		}
		/* If RSMRST_L is asserted, we're no longer in S5. */
		if (!power_has_signals(IN_PGOOD_ALL_CORE))
			return POWER_S5G3;
		break;

	default:
		break;
	}

	return common_intel_x86_power_handle_state(state);
}
