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
#include "power.h"
#include "timer.h"
#include "usb_pd.h"
#include "util.h"

#define BAT_LEVEL_PD_LIMIT 85
#define JC_TEMP_TARGET 80
#define JC_TEMP_ERR 4

#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

static timestamp_t thermal_wait_until;
static int throttled_ma = PD_MAX_CURRENT_MA;
static int prev_charge_ma;
static int prev_charge_mv;

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

static void board_set_charge_limit_throttle(int charge_ma, int charge_mv)
{
	charge_set_input_current_limit(
		MIN(throttled_ma, MAX(charge_ma, CONFIG_CHARGER_INPUT_CURRENT)),
		charge_mv);
}

static void battery_thermal_control(struct charge_state_data *curr)
{
	timestamp_t now;
	int input_current, jc_temp;
	static int skip_reset;
	const int k_p = 50;

	if (charge_manager_get_charger_voltage() == 5000 ||
	    curr->state != ST_CHARGE) {
		/* not poking charger that aggressive */
		if (skip_reset)
			return;
		skip_reset = 1;
		thermal_wait_until.val = 0;
		throttled_ma = PD_MAX_CURRENT_MA;
		board_set_charge_limit_throttle(prev_charge_ma, prev_charge_mv);
		return;
	}

	skip_reset = 0;

	now = get_time();
	if (thermal_wait_until.val == 0)
		thermal_wait_until.val = now.val + (3 * SECOND);

	if (now.val < thermal_wait_until.val)
		return;

	/* If we fail to read adc, skip for this cycle. */
	if (rt946x_get_adc(MT6370_ADC_TEMP_JC, &jc_temp))
		return;

	/* If we fail to read input curr limit, skip for this cycle. */
	if (charger_get_input_current(&input_current))
		return;

	/*
	 * If input current limit is maximum, and we are under thermal budget,
	 * just skip.
	 */
	if (input_current == PD_MAX_CURRENT_MA &&
	    jc_temp < JC_TEMP_TARGET + JC_TEMP_ERR)
		return;

	/* If the temp is within +- JC_TEMP_ERR, thermal is under control */
	if (jc_temp < JC_TEMP_TARGET + JC_TEMP_ERR &&
	    jc_temp > JC_TEMP_TARGET - JC_TEMP_ERR)
		return;

	/* PID algorithm, and operates on only P value. */
	throttled_ma = MIN(PD_MAX_CURRENT_MA,
			   input_current + k_p * (JC_TEMP_TARGET - jc_temp));
	board_set_charge_limit_throttle(throttled_ma, prev_charge_mv);

	thermal_wait_until.val = now.val + (3 * SECOND);
}

int command_jc(int argc, char **argv)
{
	static int prev_jc_temp;
	int jc_temp;

	if (rt946x_get_adc(MT6370_ADC_TEMP_JC, &jc_temp))
		jc_temp = prev_jc_temp;

	ccprintf("JC Temp: %d\n", jc_temp);
	prev_jc_temp = jc_temp;
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(jc, command_jc, "", "mt6370 junction temp");

/*
 * b/143318064: A workwround for mt6370 bad bulking efficiency.
 * If the delta of VBUS and VBAT(on krane, desired voltage 4.4V) is too small
 * (i.e. < 500mV), the bulking throughput will be bounded, and causing that we
 * can't drain 5V/3A when battery SoC above around 40%.
 * This function watches battery current. If we see battery current drops after
 * switching from high voltage to 5V (This will happen if we enable
 * CONFIG_USB_PD_PREFER_MV and set prefer votage to 5V), the charger will lost
 * power due to the inefficiency (e.g. switch from 9V/1.67A = 15W to 5V/3A,
 * but mt6370 would only sink less than 5V/2.4A = 12W), and we will request a
 * higher voltage PDO to prevent a slow charging time.
 */
static void battery_desired_curr_dynamic(struct charge_state_data *curr)
{
	static int prev_stable_current = CHARGE_CURRENT_UNINITIALIZED;
	static int prev_supply_voltage;
	int supply_voltage;
	int stable_current;
	int delta_current;

	if (curr->state != ST_CHARGE) {
		prev_supply_voltage = 0;
		prev_stable_current = CHARGE_CURRENT_UNINITIALIZED;
		/*
		 * Always force higher voltage on first PD negotiation.
		 * When desired power is around 15W ~ 11W, PD would pick
		 * 5V/3A initially, but mt6370 can't drain that much, and
		 * causes a low charging efficiency.
		 */
		pd_set_prefer_voltage(6000);
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
		 * Delay stable current evaluation, wait delay is proportional
		 * to delta_current.
		 */
		charge_reset_stable_current(delta_current * SECOND);
		/* Rewrite the stable current to re-evalute desired watt */
		charge_set_stable_current(prev_stable_current);

		/*
		 * do not alter current by thermal if we just raising PD
		 * voltage
		 */
		thermal_wait_until.val = get_time().val + CHARGE_STABLE_WAIT_US;

	} else {
		pd_set_prefer_voltage(5000);
	}

update_charge:
	prev_supply_voltage = supply_voltage;
	prev_stable_current = stable_current;
}

void mt6370_charger_profile_override(struct charge_state_data *curr)
{
	static int previous_chg_limit_mv;
	int chg_limit_mv = pd_get_max_voltage();

	battery_desired_curr_dynamic(curr);

	battery_thermal_control(curr);

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

void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	prev_charge_ma = charge_ma;
	prev_charge_mv = charge_mv;
	board_set_charge_limit_throttle(charge_ma, charge_mv);
}
