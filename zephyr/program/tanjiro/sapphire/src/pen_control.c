/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "chipset.h"
#include "console.h"
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

#define CHECK_INTERVAL_MS (2ULL * 60 * USEC_PER_SEC)
timestamp_t next_check_ts;
static bool startup_ok = false;

#define SOC_LOW 78
#define SOC_HIGH 82
#define SOC_READY_DELAY_MS 500

static void pchg_full_update(void);
DECLARE_DEFERRED(pchg_full_update);

static void pchg_full_update()
{
	timestamp_t now = get_time();
	int soc;

	/* pen removed */
	if (gpio_pin_get_dt(GPIO_DT_FROM_NODELABEL(gpio_pen_pres))) {
		next_check_ts.val = 0;
		pchg_shutdown();
		CPRINTS("stylus remove###");
		hook_call_deferred(&pchg_full_update_data, -1);
		return;
	}

	/* 没到 20 分钟 → return */
	if (now.val < next_check_ts.val) {
		CPRINTS("not 20 minuts###");
		hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
		return;
	}

	/* 上电 → 读电量 */
	if (!startup_ok) {
		pchg_startup();
		startup_ok = true;
	}
	k_msleep(SOC_READY_DELAY_MS);

	soc = pchg_get_battery_percent(0);

	if (soc == 0) {
		hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
		return;
	}

	if (soc <= SOC_LOW) {
		/* 继续充，保持上电 */
		CPRINTS("keep charge###");
	} else if (soc >= SOC_HIGH) {
		pchg_shutdown();
		CPRINTS("keep shutdown###");
		startup_ok = false;
	}

	next_check_ts.val = now.val + CHECK_INTERVAL_MS;
	hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
}

__override void board_pchg_full_strategy()
{
	next_check_ts.val = get_time().val + CHECK_INTERVAL_MS;
	pchg_shutdown();
	CPRINTS("charge full strategy begin###");
	hook_call_deferred(&pchg_full_update_data, USEC_PER_SEC);
}
