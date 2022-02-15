/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Herobrine chipset-specific configuration */

#include "common.h"
#include "battery.h"
#include "gpio.h"
#include "hooks.h"
#include "timer.h"
#include "usb_pd.h"

#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

/*
 * A window of PD negotiation. It starts from the Type-C state reaching
 * Attached.SNK, and eneds when the PD contract is created. The VBUS may be
 * raised anytime in this window.
 *
 * The current implementation is the worst case scenario. Every message the PD
 * negotiation is received at the last moment before timeout. The extra time
 * is added to compensate the delay internally, like the decision of the DPM.
 *
 * TODO(waihong): Cancel this timer when the PD contract is negotiated.
 */
#define PD_READY_TIMEOUT	(PD_T_SINK_WAIT_CAP + PD_T_SENDER_RESPONSE + \
				 PD_T_SINK_TRANSITION + 20 * MSEC)

#define PD_READY_POLL_DELAY	(10 * MSEC)

static timestamp_t pd_ready_timeout;

static bool pp5000_inited;

/* Called on USB PD connected */
static void board_usb_pd_connect(void)
{
	int soc = -1;

	if (!pp5000_inited && (
	    (battery_state_of_charge_abs(&soc) != EC_SUCCESS ||
	     soc < CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON))) {
		pd_ready_timeout = get_time();
		pd_ready_timeout.val += PD_READY_TIMEOUT;
	}
}
DECLARE_HOOK(HOOK_USB_PD_CONNECT, board_usb_pd_connect, HOOK_PRIO_DEFAULT);

static void wait_pd_ready(void)
{
	CPRINTS("Wait PD negotiated VBUS transition %u",
		pd_ready_timeout.le.lo);
	while (pd_ready_timeout.val && get_time().val < pd_ready_timeout.val)
		usleep(PD_READY_POLL_DELAY);
}

/* Called on AP S5 -> S3 transition */
static void board_chipset_pre_init(void)
{
	if (!pp5000_inited) {
		wait_pd_ready();
		CPRINTS("Enable 5V rail");
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_en_pp5000_s5), 1);
		pp5000_inited = true;
	}
}
DECLARE_HOOK(HOOK_CHIPSET_PRE_INIT, board_chipset_pre_init, HOOK_PRIO_DEFAULT);
