/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>

#include <ap_power/ap_power.h>
#include <ap_power/ap_power_events.h>
#include <ap_power_override_functions.h>
#include <power_signals.h>
#include <x86_power_signals.h>

#include "ap_power/ap_pwrseq_sm.h"

LOG_MODULE_DECLARE(ap_pwrseq, LOG_LEVEL_INF);

#if CONFIG_X86_NON_DSX_PWRSEQ_MTL
#define X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS 50

static void ap_board_timer_handler(struct k_timer *timer);
/* S5 inactive timer*/
K_TIMER_DEFINE(ap_board_timer, ap_board_timer_handler, NULL);

void board_ap_power_action_g3_entry(void *data)
{
	/* Turn off PCH_RMSRST to meet tPCH12 */
	power_signal_set(PWR_EC_PCH_RSMRST, 0);

	/* Turn off PRIM load switch. */
	power_signal_set(PWR_EN_PP3300_A, 0);
	/* This timer is for checking signal state */
	if (power_signal_get(PWR_RSMRST)) {
		k_timer_start(&ap_board_timer,
			K_MSEC(X86_NON_DSX_MTL_FORCE_SHUTDOWN_TO_MS),
			K_NO_WAIT);
	}
}

static void board_ap_power_action_g3_run(void *data)
{
	if (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_STARTUP)) {
		/* Turn on the PP3300_PRIM rail. */
		power_signal_set(PWR_EN_PP3300_A, 1);

		k_timer_start(&ap_board_timer,
			K_MSEC(AP_PWRSEQ_DT_VALUE(wait_signal_timeout)),
			K_NO_WAIT);
	}

	if (power_signals_on(IN_PGOOD_ALL_CORE)) {
		k_timer_stop(&ap_board_timer);
		return;
	}

	if  (IS_EVENT_SET(data, AP_PWRSEQ_EVENT_POWER_TIMEOUT)) {
		power_signal_set(PWR_EN_PP3300_A, 0);
	}
}

static void board_ap_power_action_g3_exit(void *data)
{
	k_timer_stop(&ap_board_timer);
	ap_power_ev_send_callbacks(AP_POWER_PRE_INIT);
}

AP_POWER_APP_STATE_DEFINE(AP_POWER_STATE_G3,
			  board_ap_power_action_g3_entry,
			  board_ap_power_action_g3_run,
			  board_ap_power_action_g3_exit)

static void ap_board_timer_handler(struct k_timer *timer)
{
	const struct device *dev = ap_pwrseq_get_instance();
	enum ap_pwrseq_state state;

	ap_pwrseq_get_current_state(dev, &state);
	switch(state) {
	case AP_POWER_STATE_G3:
		if (power_signal_get(PWR_EN_PP3300_A)) {
			ap_pwrseq_post_event(dev,
					     AP_PWRSEQ_EVENT_POWER_TIMEOUT);
		} else if (power_signal_get(PWR_RSMRST)) {
			LOG_WRN("RSMRST_ODL didn't go low!  Assuming G3.");
		}
		break;
	default:
		break;
	}
}
#endif /* CONFIG_X86_NON_DSX_PWRSEQ_MTL */
