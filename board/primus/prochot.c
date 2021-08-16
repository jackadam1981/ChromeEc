/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "adc.h"
#include "adc_chip.h"
#include "battery.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "charger.h"
#include "console.h"
#include "driver/charger/bq25710.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "prochot.h"
#include "task.h"

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

#define ADT_RATING  (PD_MAX_POWER_MW / 1000)
#define BATT_MAX_CONTINUE_DISCHARGE_WATT    66
#define PROCHOT_EVENT_200MS_TICK    TASK_EVENT_CUSTOM_BIT(0)

static struct battery_para batt_para;

static int cal_sys_watt(void)
{
	int Vacpacn;
	int V_iadpt;
	int IDPM;
	int W_adpt;

	Vacpacn = adc_read_channel(ADC_IADPT);
	V_iadpt = Vacpacn * 1000 / 40;
	IDPM = V_iadpt / CONFIG_CHARGER_SENSE_RESISTOR;
	W_adpt = IDPM * 20 / 97 * 100;

	return W_adpt;
}

static void get_batt_parameter(void)
{
	int battery_voltage;
	int battery_current;
	int battery_design_voltage;
	int battery_design_capacity;
	int flags;

	sb_read(SB_VOLTAGE, &battery_voltage);

	if (sb_read(SB_CURRENT, &battery_current))
		flags |= BATT_FLAG_BAD_CURRENT;
	else
		battery_current = (int16_t)battery_current;

	/* calculate battery wattage and convert to mW */
	batt_para.battery_continue_discharge_wattage =
		(battery_voltage * battery_current) / 1000;

	sb_read(SB_DESIGN_VOLTAGE, &battery_design_voltage);
	sb_read(SB_DESIGN_CAPACITY, &battery_design_capacity);
	batt_para.battery_design_wattage =
		(battery_design_voltage * battery_design_capacity) / 1000;

	sb_read(SB_RELATIVE_STATE_OF_CHARGE, &batt_para.state_of_charge);
}

static int get_chg_watt(void)
{
	int adapter_current;
	int adapter_voltage;
	int adapter_wattage;

	adapter_current = charge_manager_get_charger_current();
	adapter_voltage = charge_manager_get_charger_voltage();
	adapter_wattage = adapter_current * adapter_voltage / 1000 / 1000;

	return adapter_wattage;
}

static void assert_prochot(void)
{
	int adapter_wattage;
	int reg;
	int total_W;
	int adpt_W;

	/* no AC, don't control PROCHOT */
	if (!extpower_is_present()) {
		gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		return;
	}

	/* Step1, set 0x12 bit4=1 */
	if (charger_get_option(&reg))
		CPRINTS("Failed to read bq25720");

	reg |= BQ25710_CHARGE_OPTION_0_IADP_GAIN;
	charger_set_option(reg);

	/* Step2. Calculate actul system W */
	adpt_W = cal_sys_watt();

	get_batt_parameter();
	/* When battery is discharge, the battery current will be negative*/
	if (batt_para.battery_continue_discharge_wattage < 0) {
		total_W = adpt_W +
			ABS(batt_para.battery_continue_discharge_wattage);
	} else {
		/* we won't assert prochot when battery is charging. */
		total_W = adpt_W;
	}
	total_W /= 1000;

	/* Get adapter wattage */
	adapter_wattage = get_chg_watt();

	if (adapter_wattage >= ADT_RATING) {
		/* if adapter >= 60W */
		/* if no battery or battery < 10% */
		if (!battery_hw_present() || batt_para.state_of_charge <= 10) {
			if (total_W > 63)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W <= 60)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		} else {
			/* if battery >= 57W */
			if (batt_para.battery_design_wattage >= 57000) {
				if (total_W > 126)
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
				else if (total_W < 119)
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			} else {
				if (total_W > 120)
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
				else if (total_W < 114)
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			}
		}
	} else {
		/* if adapter < 60W */
		/* if no battery or battery < 10% */
		if (!battery_hw_present() || batt_para.state_of_charge <= 10) {
			if (total_W > (adapter_wattage * 105/100))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W < (adapter_wattage * 90/100))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		} else {
			/* AC + battery */
			if (total_W > (adapter_wattage +
				BATT_MAX_CONTINUE_DISCHARGE_WATT))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W < (adapter_wattage +
				(BATT_MAX_CONTINUE_DISCHARGE_WATT *
					90/100)))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		}
	}
}

/* Called by hook task every 200 ms */
static void control_prochot_tick(void)
{
	task_set_event(TASK_ID_PROCHOT, PROCHOT_EVENT_200MS_TICK);
}
DECLARE_HOOK(HOOK_TICK, control_prochot_tick, HOOK_PRIO_DEFAULT);

void prochot_task(void *u)
{
	uint32_t evt;

	while (1) {
		evt = task_wait_event(-1);

		if (evt & PROCHOT_EVENT_200MS_TICK)
			assert_prochot();
	}
}
