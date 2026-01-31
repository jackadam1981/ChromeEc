/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "gpio/gpio_int.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "peripheral_charger.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>

#include <ap_power/ap_power.h>

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

__override void board_pchg_power_on(int port, bool on)
{
	if (port != 0)
		return;

	gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_pp5000_wlc_en), on);
}

#define CHECK_INTERVAL_MS (20ULL * 60 * USEC_PER_SEC)
timestamp_t next_check_ts;
static bool startup_ok = false;
static bool recharging = false;
static int soc_fail_cnt;

#define SOC_FAIL_MAX 20
#define SOC_LOW 80
#define SOC_FULL 97

static void pchg_full_update(void);
DECLARE_DEFERRED(pchg_full_update);

static void pchg_full_update()
{
	timestamp_t now = get_time();
	int soc;

	int system_off =
		chipset_in_or_transitioning_to_state(CHIPSET_STATE_ANY_OFF);

	/* pen removed */
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_pen_pres)) ||
	    (system_off && !extpower_is_present())) {
		pchg_shutdown();
		startup_ok = false;
		recharging = false;
		next_check_ts.val = 0;

		hook_call_deferred(&pchg_full_update_data, -1);
		CPRINTS("stylus: remove");
		return;
	}

	/* wait 20m delay */
	if (now.val < next_check_ts.val) {
		CPRINTS("stylus: after 20m check");
		hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
		return;
	}

	/* power on wireless charge */
	if (!startup_ok) {
		pchg_startup();
		startup_ok = true;
	}

	/* read stylus percent */
	soc = pchg_get_battery_percent(0);
	if (soc <= 0 || soc > 100) {
		soc_fail_cnt++;
		CPRINTS("stylus: soc read fail (%d)", soc_fail_cnt);
		if (soc_fail_cnt >= SOC_FAIL_MAX) {
			if (startup_ok) {
				pchg_shutdown();
				startup_ok = false;
			}
			soc_fail_cnt = 0;
		}
		hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
		return;
	}
	soc_fail_cnt = 0;

	/* recharge begin */
	if (!recharging && soc <= SOC_LOW) {
		recharging = true;
		CPRINTS("stylus: enter recharge soc=%d", soc);
	}

	if (recharging && soc >= SOC_FULL) {
		recharging = false;
		CPRINTS("stylus: recharge done soc=%d", soc);
	}

	/* enforce hardware state */
	if (recharging) {
		/* keep power on */
		if (!startup_ok) {
			pchg_startup();
			startup_ok = true;
		}
	} else {
		if (startup_ok) {
			pchg_shutdown();
			startup_ok = false;
			CPRINTS("stylus: charge full then discharge");
		}
	}

	next_check_ts.val = now.val + CHECK_INTERVAL_MS;
	hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
}

__override void board_pchg_full_strategy()
{
	next_check_ts.val = get_time().val + CHECK_INTERVAL_MS;
	pchg_shutdown();
	startup_ok = false;
	recharging = false;
	CPRINTS("stylus: charge full strategy begin");
	hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
}
