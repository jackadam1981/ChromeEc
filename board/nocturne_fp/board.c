/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* Meowth Fingerprint MCU configuration */

#include "common.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"

/**
 * Disable restricted commands when the system is locked.
 *
 * @see console.h system.c
 */
int console_is_restricted(void)
{
	return system_is_locked();
}

#include "gpio_list.h"

/* Allow changing the signal used for alt sleep depending on the board being
 * used: http://b/179946521.
 */
static int gpio_slp_alt_l = GPIO_SLP_ALT_L;

static void ap_deferred(void)
{
	/*
	 * Behavior:
	 * AP Active  (ex. Intel S0):   SLP_L is 1
	 * AP Suspend (ex. Intel S0ix): SLP_L is 0
	 * The alternative SLP_ALT_L should be pulled high at all the times.
	 *
	 * Legacy Intel behavior:
	 * in S3:   SLP_ALT_L is 0 and SLP_L is X.
	 * in S0ix: SLP_ALT_L is X and SLP_L is 0.
	 * in S0:   SLP_ALT_L is 1 and SLP_L is 1.
	 * in S5/G3, the FP MCU should not be running.
	 */
	int running = gpio_get_level(gpio_slp_alt_l) &&
		      gpio_get_level(GPIO_SLP_L);

	if (running) { /* AP is S0 */
		disable_sleep(SLEEP_MASK_AP_RUN);
		hook_notify(HOOK_CHIPSET_RESUME);
	} else { /* AP is suspend/S0ix/S3 */
		hook_notify(HOOK_CHIPSET_SUSPEND);
		enable_sleep(SLEEP_MASK_AP_RUN);
	}
}
DECLARE_DEFERRED(ap_deferred);

/* PCH power state changes */
void slp_event(enum gpio_signal signal)
{
	hook_call_deferred(&ap_deferred_data, 0);
}

/* Initialize board. */
static void board_init(void)
{
	/* Use SPI select as a proxy for running on the icetower dev board. */
	// if (spi_select == FP_SENSOR_SPI_SELECT_DEVELOPMENT)
	// 	gpio_slp_alt_l = GPIO_SLP_ALT_DEV_L;

	/* Enable interrupt on PCH power signals */
	gpio_enable_interrupt(gpio_slp_alt_l);
	gpio_enable_interrupt(GPIO_SLP_L);

	/*
	 * Enable the SPI slave interface if the PCH is up.
	 * Do not use hook_call_deferred(), because ap_deferred() will be
	 * called after tasks with priority higher than HOOK task (very late).
	 */
	ap_deferred();

	if (IS_ENABLED(SECTION_IS_RW)) {
		board_init_rw();
	}
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
