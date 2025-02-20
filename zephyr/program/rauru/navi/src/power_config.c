/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state.h"
#include "common.h"
#include "console.h"
#include "hooks.h"
#include "math_util.h"
#include "power.h"
#include "usb_pd.h"
#include "util.h"

#include <dt-bindings/battery.h>

#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

static int charge_voltage_backup = 0;

void navi_batt_full(void)
{
	int batt_state;
	int active_port = charge_manager_get_active_charge_port();

	battery_status(&batt_state);

	if (charge_voltage_backup == 0)
		charge_voltage_backup = charge_manager_get_charger_voltage();
	CPRINTS("charge_voltage_backup = %d", charge_voltage_backup);

	if (batt_state & SB_STATUS_FULLY_CHARGED) {
		pd_request_source_voltage(active_port, 15000);
		pd_dpm_request(active_port, DPM_REQUEST_NEW_POWER_LEVEL);
	} else {
		pd_request_source_voltage(active_port, charge_voltage_backup);
		pd_dpm_request(active_port, DPM_REQUEST_NEW_POWER_LEVEL);
	}
}
DECLARE_HOOK(HOOK_BATTERY_FULL, navi_batt_full, HOOK_PRIO_DEFAULT);
