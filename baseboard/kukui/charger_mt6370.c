/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "charge_manager.h"
#include "charge_state_v2.h"
#include "charger_mt6370.h"
#include "console.h"
#include "driver/charger/rt946x.h"
#include "driver/tcpm/mt6370.h"
#include "hooks.h"
#include "math_util.h"
#include "power.h"
#include "timer.h"
#include "usb_pd.h"
#include "util.h"

#define BAT_LEVEL_PD_LIMIT 85
#define JC_TEMP_TARGET 80
#define JC_TEMP_ERR 5

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)

#ifndef CONFIG_BATTERY_SMART
int board_cut_off_battery(void)
{
	/* The cut-off procedure is recommended by Richtek. b/116682788 */
	rt946x_por_reset();
	mt6370_vconn_discharge(0);
	rt946x_cutoff_battery();

	return EC_SUCCESS;
}
#endif

static int throttled_ma = PD_MAX_CURRENT_MA;
static int prev_charge_ma;
static int prev_charge_mv;

static void board_set_charge_limit_throttle(int charge_ma, int charge_mv)
{
	charge_set_input_current_limit(
		MIN(throttled_ma, MAX(charge_ma, CONFIG_CHARGER_INPUT_CURRENT)),
		charge_mv);
}

void board_set_charge_limit(int port, int supplier, int charge_ma, int max_ma,
			    int charge_mv)
{
	prev_charge_ma = charge_ma;
	prev_charge_mv = charge_mv;
	board_set_charge_limit_throttle(charge_ma, charge_mv);
}

/* Dynamicly change current based on battery's desired watt */
void battery_desired_curr_dynamic(struct charge_state_data *curr)
{
	static int prev_stable_current = CHARGE_CURRENT_UNINITIALIZED;
	static int prev_supply_voltage;
	int supply_voltage;
	int stable_current;
	int delta_current;

	if (curr->state != ST_CHARGE) {
		prev_supply_voltage = 0;
		prev_stable_current = CHARGE_CURRENT_UNINITIALIZED;
		pd_set_prefer_voltage(5000);
		return;
	}

	supply_voltage = charge_manager_get_charger_voltage();
	stable_current = charge_get_stable_current();

	if (stable_current == CHARGE_CURRENT_UNINITIALIZED)
		return;

	if (!prev_supply_voltage)
		goto update_charge;

	delta_current = prev_stable_current - stable_current;
	if (curr->batt.state_of_charge >= BATTERY_CV_LEVEL &&
	    supply_voltage == 5000 && prev_supply_voltage > supply_voltage &&
	    prev_stable_current - stable_current > 300) {
		/* Raise perfer voltage above 5000mV */
		pd_set_prefer_voltage(6000);
		/*
		 * Delay stable current evaluation, wait delay is proportional to
		 * delta_current.
		 */
		charge_reset_stable_current(delta_current * SECOND);
		/* Rewrite the stable current to re-evalute desired watt */
		charge_set_stable_current(prev_stable_current);
	} else {
		pd_set_prefer_voltage(5000);
	}

update_charge:
	prev_supply_voltage = supply_voltage;
	prev_stable_current = stable_current;
}

int command_jc(int argc, char **argv);

void battery_thermal_control(struct charge_state_data *curr)
{
	static timestamp_t wait_until;
	timestamp_t now;
	int input_current, jc_temp;
	const int k_p = 50;

	if (charge_manager_get_charger_voltage() == 5000 ||
	    curr->state != ST_CHARGE) {
		throttled_ma = PD_MAX_CURRENT_MA;
		board_set_charge_limit_throttle(prev_charge_ma, prev_charge_mv);
		wait_until.val = 0;
		return ;
	}

	now = get_time();
	if (wait_until.val == 0)
		wait_until.val = now.val + (3 * SECOND);

	if (now.val < wait_until.val)
		return;

	/* If we fail to read adc, skip for this cycle. */
	if (rt946x_get_adc(MT6370_ADC_TEMP_JC, &jc_temp))
		return;

	/* If the temp is within +- JC_TEMP_ERR, just leave.*/
	if (jc_temp < JC_TEMP_TARGET + JC_TEMP_ERR &&
	    jc_temp > JC_TEMP_TARGET - JC_TEMP_ERR)
		return;


	if (charger_get_input_current(&input_current))
		return;

	/* PID algorithm, and operates on only P value. */
	throttled_ma = MIN(PD_MAX_CURRENT_MA,
			   input_current + k_p * (JC_TEMP_TARGET - jc_temp));
	board_set_charge_limit_throttle(throttled_ma, prev_charge_mv);

	wait_until.val = now.val + (3 * SECOND);
}

int command_jc(int argc, char **argv)
{
	int jc_temp;
#if 0
	const char *red = "\033[31m";
	const char *green = "\033[32m";
	const char *yellow = "\033[33m";
	const char *color;
#endif

	if (rt946x_get_adc(MT6370_ADC_TEMP_JC, &jc_temp)) {
		CPRINTS("\033[31mJC temp error\033[0m");
		return EC_ERROR_BUSY;
	}

#if 0
	const char *red = "\033[31m";
	if (jc_temp <= 75)
		color = green;
	else if (jc_temp <= 85)
		color = yellow;
	else
		color = red;
#endif
	//CPRINTS("JC temp: %s%d\033[0m",color, jc_temp);
	CPRINTS("JC temp: %d", jc_temp);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(jc, command_jc, "", "");

void mt6370_charger_profile_override(struct charge_state_data *curr)
{
	static int previous_chg_limit_mv;
	int chg_limit_mv = pd_get_max_voltage();

	battery_desired_curr_dynamic(curr);

	battery_thermal_control(curr);

	//command_jc(0, NULL);

	/* Limit input (=VBUS) to 5V when soc > 85% and charge current < 1A. */
	if (!(curr->batt.flags & BATT_FLAG_BAD_CURRENT) &&
	    charge_get_percent() > BAT_LEVEL_PD_LIMIT &&
	    curr->batt.current < 1000 && power_get_state() != POWER_S0)
		chg_limit_mv = 5500;
	else
		chg_limit_mv = PD_MAX_VOLTAGE_MV;

	if (chg_limit_mv != previous_chg_limit_mv)
		CPRINTS("VBUS limited to %dmV", chg_limit_mv);
	previous_chg_limit_mv = chg_limit_mv;

	/* Pull down VBUS */
	if (pd_get_max_voltage() != chg_limit_mv)
		pd_set_external_voltage_limit(0, chg_limit_mv);

	/*
	 * When the charger says it's done charging, even if fuel gauge says
	 * SOC < BATTERY_LEVEL_NEAR_FULL, we'll overwrite SOC with
	 * BATTERY_LEVEL_NEAR_FULL. So we can ensure both Chrome OS UI
	 * and battery LED indicate full charge.
	 *
	 * Enable this hack on on-board gauge only (b/142097561)
	 */
	if (IS_ENABLED(CONFIG_BATTERY_MAX17055) && rt946x_is_charge_done()) {
		curr->batt.state_of_charge = MAX(BATTERY_LEVEL_NEAR_FULL,
						 curr->batt.state_of_charge);
	}

}

static void board_charge_termination(void)
{
	static uint8_t te;
	/* Enable charge termination when we are sure battery is present. */
	if (!te && battery_is_present() == BP_YES) {
		if (!rt946x_enable_charge_termination(1))
			te = 1;
	}
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE,
	     board_charge_termination,
	     HOOK_PRIO_DEFAULT);

__override int board_get_desired_mw(void)
{
	return PLT_SHIFT_MW + charge_get_desired_mw();
}
