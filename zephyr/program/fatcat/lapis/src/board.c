/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "charge_state.h"
#include "charger.h"
#include "common.h"
#include "cros_board_info.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "lid_switch.h"

#include <zephyr/logging/log.h>
#include <zephyr/sys_clock.h>

#include <ap_power/ap_pwrseq.h>
#include <usbc/pdc_power_mgmt.h>

LOG_MODULE_REGISTER(lapis, LOG_LEVEL_INF);

static void set_chg_reg_custom(void)
{
	charger_set_frequency(808);
}
DECLARE_HOOK(HOOK_INIT, set_chg_reg_custom, HOOK_PRIO_POST_BATTERY_INIT + 1);

static void tp_enable(void)
{
	if (lid_is_open()) {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_tp_disable), true);
	} else {
		gpio_pin_set_dt(GPIO_DT_FROM_NODELABEL(gpio_tp_disable), false);
	}
}
DECLARE_HOOK(HOOK_LID_CHANGE, tp_enable, HOOK_PRIO_DEFAULT);

static void disable_sleep_bid(void)
{
	uint32_t board_id = 0;
	/* Errors will count as board_id 0 */
	cbi_get_board_version(&board_id);

	if (board_id > 1)
		enable_sleep(SLEEP_MASK_FORCE_NO_DSLEEP);
	else
		disable_sleep(SLEEP_MASK_FORCE_NO_DSLEEP);
}
DECLARE_HOOK(HOOK_INIT, disable_sleep_bid, HOOK_PRIO_POST_I2C);

/* Lower input voltage to 9V in S5/G3 when battery is full. */
#define PD_VOLTAGE_WHEN_FULL 9000
static void control_voltage(void);
DECLARE_DEFERRED(control_voltage);
static void control_voltage(void)
{
	int p;
	int batt_status;

	if (!extpower_is_present()) {
		pd_set_max_voltage(CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV);
		hook_call_deferred(&control_voltage_data, -1);
		return;
	}

	if (battery_status(&batt_status)) {
		LOG_ERR("Failed to get battery status");
		hook_call_deferred(&control_voltage_data, 5 * USEC_PER_SEC);
		return;
	}

	if (batt_status & STATUS_FULLY_CHARGED) {
		p = charge_manager_get_active_charge_port();

		if ((p > CHARGE_PORT_NONE) &&
		    (pd_get_max_voltage() > PD_VOLTAGE_WHEN_FULL)) {
			LOG_INF("Set port%d input %dmV", p,
				PD_VOLTAGE_WHEN_FULL);
			pd_request_source_voltage(p, PD_VOLTAGE_WHEN_FULL);
		}

		hook_call_deferred(&control_voltage_data, -1);
		return;
	}

	/* Every 5 second to check battery fully charged status. */
	hook_call_deferred(&control_voltage_data, 5 * USEC_PER_SEC);
}

static void trigger_control(void)
{
	if (!chipset_in_state(CHIPSET_STATE_ANY_OFF) ||
	    (battery_is_present() != BP_YES))
		return;

	/* Must delay for PD transform when AC on/off. */
	hook_call_deferred(&control_voltage_data, 5 * USEC_PER_SEC);
}
DECLARE_HOOK(HOOK_AC_CHANGE, trigger_control, HOOK_PRIO_DEFAULT + 1);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, trigger_control, HOOK_PRIO_DEFAULT + 1);

static void disable_control_voltage(void)
{
	static int boot_flag;
	int p;

	if (!boot_flag) {
		boot_flag++;
		return;
	}

	p = charge_manager_get_active_charge_port();

	if (p != CHARGE_PORT_NONE) {
		if (pd_get_max_voltage() <
		    CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV) {
			pd_request_source_voltage(
				p, CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV);
		}
	}

	hook_call_deferred(&control_voltage_data, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, disable_control_voltage, HOOK_PRIO_DEFAULT);
