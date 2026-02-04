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

static void control_snk_pdo(void);
DECLARE_DEFERRED(control_snk_pdo);
static void control_snk_pdo(void)
{
	int p;
	unsigned int mv;
	int batt_status;

	if (!extpower_is_present()) {
		printk("--no AC\n");
		pd_set_max_voltage(CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV);
		hook_call_deferred(&control_snk_pdo_data, -1);
		return;
	}

	if (battery_status(&batt_status)) {
		printk("--read battery status fail\n");
	}

	if (batt_status & STATUS_FULLY_CHARGED) {
		p = charge_manager_get_active_charge_port();

		mv = pd_get_max_voltage();
		printk("----mv=%d\n", mv);

		if (mv > 9000) {
			printk("--set port%d mv=9V\n", p);
			pd_request_source_voltage(p, 9000);
		}

		hook_call_deferred(&control_snk_pdo_data, -1);
		return;
	}

	printk("--5s detect\n");
	hook_call_deferred(&control_snk_pdo_data, 5 * USEC_PER_SEC);
}

static void trigger_check_pdo(void)
{
	const struct device *dev = ap_pwrseq_get_instance();
	enum ap_pwrseq_state state = ap_pwrseq_get_current_state(dev);

	printk("------trigger_check_pdo\n");

	if ((state != AP_POWER_STATE_G3) && (state != AP_POWER_STATE_S5)) {
		printk("--no S5/G3\n");
		return;
	}

	if (battery_is_present() != BP_YES) {
		printk("--no battery present\n");
		return;
	}

	hook_call_deferred(&control_snk_pdo_data, 5 * USEC_PER_SEC);
}
DECLARE_HOOK(HOOK_AC_CHANGE, trigger_check_pdo, HOOK_PRIO_DEFAULT + 1);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, trigger_check_pdo, HOOK_PRIO_DEFAULT + 1);

static void disable_check_pdo(void)
{
	static int boot_flag;
	int p;

	if (!boot_flag) {
		boot_flag++;
		return;
	}

	p = charge_manager_get_active_charge_port();
	printk("--startup check charge port%d\n", p);

	if (p != CHARGE_PORT_NONE) {
		if (pd_get_max_voltage() <
		    CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV) {
			printk("--startup port%d mv=20v\n", p);
			pd_request_source_voltage(
				p, CONFIG_PLATFORM_EC_USB_PD_MAX_VOLTAGE_MV);
		}
	}

	hook_call_deferred(&control_snk_pdo_data, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, disable_check_pdo, HOOK_PRIO_DEFAULT);
