/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "battery.h"
#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "charger.h"
#include "chipset.h"
#include "drivers/ucsi_v3.h"
#include "hooks.h"
#include "math_util.h"
#include "usbc/pdc_dpm.h"
#include "usbc/pdc_power_mgmt.h"

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(board_thermal, LOG_LEVEL_INF);

#define BATTERY_MAX_DISCHARGE_CURRENT 4500
#define RETRY_TIMES 5
static int port_status[2] = { 0 }; // 1:max port change  0: max_port recovery

static int battery_discharge(void)
{
	int batt_status;

	return battery_status(&batt_status) ?
		       0 :
		       !!(batt_status & STATUS_DISCHARGING);
}

static int get_max_port(void)
{
	int i;
	bool any_port_is_source = false;
	int max_port = -1;

	for (i = 0; i < board_get_usb_pd_port_count(); i++) {
		if (pd_get_power_role(i) == PD_ROLE_SOURCE) {
			any_port_is_source = true;
			if (pdc_dpm_get_source_current(i) == 3000) {
				max_port = i;
				break;
			}
		}
	}

	if (!any_port_is_source)
		return -1;

	if (max_port < 0 || max_port >= board_get_usb_pd_port_count())
		return -1;

	return max_port;
}

static void decrease_usbc_src_output(void);
DECLARE_DEFERRED(decrease_usbc_src_output);

static void decrease_usbc_src_output(void)
{
	struct batt_params batt;
	static int16_t avg_current;
	static int cnt;
	int port;

	battery_get_params(&batt);
	avg_current += (batt.current & 0xffff);

	if (++cnt <= RETRY_TIMES) {
		hook_call_deferred(&decrease_usbc_src_output_data,
				   100 * USEC_PER_MSEC);
		return;
	}

	avg_current = ABS(avg_current / RETRY_TIMES);
	LOG_INF("avg_current = %d", avg_current);

	port = get_max_port();

	if (avg_current > BATTERY_MAX_DISCHARGE_CURRENT) {
		enum usb_typec_current_t rp = TC_CURRENT_1_5A;

		if (port == -1) {
			LOG_WRN("No need decrease typec port output, skip set current limit");
			cnt = 0;
			avg_current = 0;
			hook_call_deferred(&decrease_usbc_src_output_data,
					   100 * USEC_PER_MSEC);
			return;
		}

		if (port_status[port] != 1) {
			LOG_INF("Detect Battery discharge over, set rp=%d", rp);
			pdc_power_mgmt_set_current_limit(port, rp);
		}
		cnt = 0;
		avg_current = 0;
		port_status[port] = 1;
		hook_call_deferred(&decrease_usbc_src_output_data,
				   100 * USEC_PER_MSEC);
	} else {
		enum usb_typec_current_t rp = TC_CURRENT_3_0A;

		if (get_max_port() == -1) {
			LOG_WRN("No need increase typec port output, skip set current limit");
			cnt = 0;
			avg_current = 0;
			hook_call_deferred(&decrease_usbc_src_output_data,
					   100 * USEC_PER_MSEC);
			return;
		}

		if (port_status[port] != 0) {
			LOG_INF("Detect Battery discharge recovery, set rp=%d",
				rp);
			pdc_power_mgmt_set_current_limit(get_max_port(), rp);
		}
		cnt = 0;
		avg_current = 0;
		port_status[port] = 0;
		hook_call_deferred(&decrease_usbc_src_output_data,
				   100 * USEC_PER_MSEC);
	}
}

static void update_typec_ilim()
{
	if (chipset_in_state(CHIPSET_STATE_ON)) {
		if (battery_discharge()) {
			LOG_INF("##Battery discharge");
			hook_call_deferred(&decrease_usbc_src_output_data,
					   100 * USEC_PER_MSEC);
		} else {
			LOG_INF("##Battery not discharge");
			hook_call_deferred(&decrease_usbc_src_output_data, -1);
		}
	}
}
DECLARE_HOOK(HOOK_SECOND, update_typec_ilim, HOOK_PRIO_DEFAULT);
