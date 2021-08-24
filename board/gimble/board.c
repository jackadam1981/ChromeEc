/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "battery.h"
#include "battery_smart.h"
#include "button.h"
#include "charge_ramp.h"
#include "charge_state_v2.h"
#include "charge_manager.h"
#include "charger.h"
#include "common.h"
#include "compile_time_macros.h"
#include "console.h"
#include "driver/charger/bq25710.h"
#include "gpio.h"
#include "gpio_signal.h"
#include "hooks.h"
#include "driver/accel_bma2x2_public.h"
#include "driver/accel_bma422.h"
#include "driver/accelgyro_bmi160.h"
#include "driver/accelgyro_lsm6dsm.h"
#include "fw_config.h"
#include "hooks.h"
#include "i2c.h"
#include "math_util.h"
#include "lid_switch.h"
#include "power_button.h"
#include "power.h"
#include "registers.h"
#include "switch.h"
#include "tablet_mode.h"
#include "throttle_ap.h"
#include "usbc_config.h"

#include "gpio_list.h" /* Must come after other header files. */

/* Console output macros */
#define CPRINTF(format, args...) cprintf(CC_CHARGER, format, ## args)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)

/******************************************************************************/
/* USB-A charging control */

const int usb_port_enable[USB_PORT_COUNT] = {
	GPIO_EN_PP5000_USBA_R,
};
BUILD_ASSERT(ARRAY_SIZE(usb_port_enable) == USB_PORT_COUNT);

/******************************************************************************/

__override void board_cbi_init(void)
{
	config_usb_db_type();
}

/* Called on AP S3 -> S0 transition */
static void board_chipset_resume(void)
{
	/* Allow keyboard backlight to be enabled */

	/* TODO(b/190783131)
	 * Need to implement specific keyboard backlight control method.
	 */
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

/* Called on AP S0 -> S3 transition */
static void board_chipset_suspend(void)
{
	/* Turn off the keyboard backlight if it's on. */

	/* TODO(b/190783131)
	 * Need to implement specific keyboard backlight control method.
	 */
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CHARGE_RAMP_SW

/*
 * TODO(b/181508008): tune this threshold
 */

#define BC12_MIN_VOLTAGE 4400

/**
 * Return true if VBUS is too low
 */
int board_is_vbus_too_low(int port, enum chg_ramp_vbus_state ramp_state)
{
	int voltage;

	if (charger_get_vbus_voltage(port, &voltage))
		voltage = 0;

	if (voltage == 0) {
		CPRINTS("%s: must be disconnected", __func__);
		return 1;
	}

	if (voltage < BC12_MIN_VOLTAGE) {
		CPRINTS("%s: port %d: vbus %d lower than %d", __func__,
			port, voltage, BC12_MIN_VOLTAGE);
		return 1;
	}

	return 0;
}

#endif /* CONFIG_CHARGE_RAMP_SW */

enum battery_present battery_hw_present(void)
{
	enum gpio_signal batt_pres;

	batt_pres = GPIO_EC_BATT_PRES_ODL;

	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(batt_pres) ? BP_NO : BP_YES;
}

static void board_init(void)
{
	/* The PPVAR_SYS must same as battery voltage(3 cells * 4.4V) */
	if (extpower_is_present() && battery_hw_present()) {
		bq25710_set_min_system_voltage(CHARGER_SOLO, 9200);
	} else {
		bq25710_set_min_system_voltage(CHARGER_SOLO, 13200);
	}
}
DECLARE_HOOK(HOOK_SECOND, board_init, HOOK_PRIO_DEFAULT);

__override void board_set_charge_limit(int port, int supplier, int charge_ma,
			    int max_ma, int charge_mv)
{
	/*
	 * Follow OEM request to limit the input current to
	 * 90% negotiated limit.
	 */
	charge_ma = charge_ma * 90 / 100;

	charge_set_input_current_limit(MAX(charge_ma,
					CONFIG_CHARGER_INPUT_CURRENT),
					charge_mv);
}

static void assert_prochot(void)
{
	int adapter_rating;
	int adapter_voltage;
	int adapter_current;
	int adapter_wattage;
	int battery_voltage;
	int battery_current;
	int battery_continue_discharge_wattage;
	int battery_max_continue_discharge_wattage;
	int flags;
	int IDPM;
	int reg;
	int state_of_charge;
	int total_W;
	int Vacpacn;
	int V_iadpt;
	int W_adpt;

	/* Step1, set 0x12 bit4=1 */
	if (i2c_read16(I2C_PORT_CHARGER, BQ25710_SMBUS_ADDR1_FLAGS,
		       BQ25710_REG_CHARGE_OPTION_0, &reg) == EC_SUCCESS) {
		reg |= BQ25710_CHARGE_OPTION_0_IADP_GAIN;
		/* if AC only, disable IDPM*/
		if (!battery_hw_present())
			reg &= ~BQ25710_CHARGE_OPTION_0_EN_IDPM;
		else
			reg |= BQ25710_CHARGE_OPTION_0_EN_IDPM;
	} else
		CPRINTS("Failed to read bq25720");

	if (i2c_write16(I2C_PORT_CHARGER, BQ25710_SMBUS_ADDR1_FLAGS,
				BQ25710_REG_CHARGE_OPTION_0, reg))
		CPRINTS("Failed to set bq25720");

	/* Step2. Calculate actul system W */
	Vacpacn = adc_read_channel(ADC_IADPT);

	/* the ratio selectable through IADPT_GAIN bit. */
	V_iadpt = Vacpacn * 1000 / 40;

	IDPM = V_iadpt / CONFIG_CHARGER_SENSE_RESISTOR;

	W_adpt = IDPM * 20 / 97 * 100;

	/* Step3. read battery voltage/ current */
	sb_read(SB_VOLTAGE, &battery_voltage);

	if (sb_read(SB_CURRENT, &battery_current))
		flags |= BATT_FLAG_BAD_CURRENT;
	else
		battery_current = (int16_t)battery_current;

	/* calculate battery wattage and convert to mW */
	battery_continue_discharge_wattage =
		(battery_voltage * battery_current) / 1000;

	/* When battery is discharge, the battery current will be negative*/
	if (battery_continue_discharge_wattage < 0) {
		battery_continue_discharge_wattage =
			ABS(battery_continue_discharge_wattage);
		total_W = W_adpt + battery_continue_discharge_wattage;
	} else {
		/* we won't assert prochot when battery is charged. */
		total_W = W_adpt;
	}
	total_W /= 1000;
	/* max contunue discharge wattage is defined in battery spec. */
	battery_max_continue_discharge_wattage = 45;

	adapter_rating = PD_MAX_POWER_MW / 1000;

	sb_read(SB_RELATIVE_STATE_OF_CHARGE, &state_of_charge);

	/* Get adapter wattage */
	adapter_current = charge_manager_get_charger_current();
	adapter_voltage = charge_manager_get_charger_voltage();
	adapter_wattage = adapter_current * adapter_voltage / 1000 / 1000;

	if (!extpower_is_present()) {
		if (!battery_hw_present()) {
			gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		} else {
			battery_continue_discharge_wattage =
				ABS(battery_continue_discharge_wattage);
			if ((battery_continue_discharge_wattage / 1000) > 43)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if ((battery_continue_discharge_wattage/1000) < 38)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		}
		return;
	}
	if (adapter_wattage >= adapter_rating) {
		/* if adapter >= 60 */
		/* if no battery or battery < 10% */
		if (!battery_hw_present() || state_of_charge <= 10) {
			if (total_W > 63)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W <= 60)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		} else {
			/* AC + battery */
			if (total_W > 120)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W < 114)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		}
	} else {
		/* if adapter < 60 */
		/* if no battery or battery < 10% */
		if (!battery_hw_present() || state_of_charge <= 10) {
			if (total_W > (adapter_wattage * 105/100))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W <= (adapter_wattage * 90/100))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		} else {
			/* AC + battery */
			if (total_W > (adapter_wattage +
				battery_max_continue_discharge_wattage))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			else if (total_W < (adapter_wattage +
				(battery_max_continue_discharge_wattage *
					90 / 100)))
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		}
	}
}
DECLARE_HOOK(HOOK_TICK, assert_prochot, HOOK_PRIO_DEFAULT);
