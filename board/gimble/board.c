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
#include "driver/accelgyro_bmi160.h"
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
	int IDPM;
	int V_acpacn;
	//int V_iadpt;
	int I_adpt;
	int W_adpt;
	int total_W;

	int battery_voltage;
	int battery_capacity;
	int battery_max_continue_discharge;
	//int battery_design_voltage;
	//int battery_design_capacity;
	int battery_design_wattage;
	int state_of_charge;
	int adapter_rating;

	int adapter_voltage;
	int adapter_current;
	int adapter_wattage;
	int flags;

	int reg;

	/* Step1, set 0x12 bit4=1 */
	//CPRINTS("set bq25720 bit4");
	if (i2c_read16(I2C_PORT_CHARGER, BQ25710_SMBUS_ADDR1_FLAGS,
		       BQ25710_REG_CHARGE_OPTION_0, &reg) == EC_SUCCESS) {
		reg |= BQ25710_CHARGE_OPTION_0_IADP_GAIN;
		reg |= ~BQ25710_CHARGE_OPTION_0_EN_IDPM;
	}else
		CPRINTS("Failed to read bq25720");

	if (i2c_write16(I2C_PORT_CHARGER, BQ25710_SMBUS_ADDR1_FLAGS,
				BQ25710_REG_CHARGE_OPTION_0, reg))
		CPRINTS("Failed to set bq25720");

	/* Step1. Calculate actul W */
	I_adpt = adc_read_channel(ADC_IADPT);
	//CPRINTS("I_adpt=%dmV", I_adpt);

	V_acpacn = I_adpt * 1000 / 40;
	//CPRINTS("V_acpacn=%dmV", V_acpacn);

	IDPM = V_acpacn / CONFIG_CHARGER_SENSE_RESISTOR;
	//CPRINTS("IDPM=%dmV", IDPM);

	W_adpt = IDPM * 20 / 97 * 100;
	CPRINTS("W_adpt=%dmW", W_adpt);

	/* Step2. read battery voltage/ capacity */
	sb_read(SB_VOLTAGE, &battery_voltage);
	//CPRINTS("battery_voltage=%dmV", battery_voltage);

	if (sb_read(SB_CURRENT, &battery_capacity))
		flags |= BATT_FLAG_BAD_CURRENT;
	else
		battery_capacity = (int16_t)battery_capacity;

	//sb_read(SB_CURRENT, &battery_capacity);
	//CPRINTS("battery_current=%dmA", battery_capacity);

	battery_max_continue_discharge = (battery_voltage * battery_capacity) / 1000; //convert to mW
	CPRINTS("battery_max_continue_discharge=%dmW", battery_max_continue_discharge);

	if (battery_max_continue_discharge < 0) {
		battery_max_continue_discharge = ABS(battery_max_continue_discharge);
		total_W = W_adpt + battery_max_continue_discharge;
	} else {
		total_W = W_adpt;
	}
	CPRINTS("total_W=%dmW", total_W);

	battery_design_wattage = 45;
	//CPRINTS("battery_design_wattage=%dmWh", battery_design_wattage);

	adapter_rating = PD_MAX_POWER_MW;
	//CPRINTS("adapter_rating + battery_max_continue_discharge=%dmW", adapter_rating + battery_design_wattage);

	sb_read(SB_RELATIVE_STATE_OF_CHARGE, &state_of_charge);

	adapter_current = charge_manager_get_charger_current();
	adapter_voltage = charge_manager_get_charger_voltage();
	adapter_wattage = adapter_current * adapter_voltage /1000 /1000;
	CPRINTS("adapter_wattage=%dmW", adapter_wattage);

	if (!extpower_is_present()) {
		if (!battery_hw_present()) {
			CPRINTS("no AC no batt, de-assert prochot");
			gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			return;
		} else {
			battery_max_continue_discharge = ABS(battery_max_continue_discharge);
			if(battery_max_continue_discharge > 43) {
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			} else if(battery_max_continue_discharge < 38)
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
		}
		return;
	}

	if (adapter_wattage >= adapter_rating) {
		//CPRINTS("ac >= 60w");
		if (!battery_hw_present() || state_of_charge <= 10){
			if((total_W / 1000) > 63) {
				CPRINTS("no batt, assert prochot");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			}else if((total_W / 1000) <= 60) {
				CPRINTS("no batt, de-assert prochot");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			}
		} else {
			if (battery_design_wattage >= 57000) {
				if((total_W / 1000) > 126) {
					CPRINTS("assert prochot1");
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
				}else if((total_W / 1000) < 119) {
					CPRINTS("de-assert prochot1");
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
				}
			} else {
				if((total_W / 1000) > 120) {
					CPRINTS("assert prochot2");
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
				}else if((total_W / 1000) < 114) {
					CPRINTS("de-assert prochot2");
					gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
				}
			}
		}
	} else {
		//CPRINTS("ac < 60w");
		if (!battery_hw_present() || state_of_charge <= 10){
			if((total_W / 1000) > (adapter_wattage *105/100)) {
				CPRINTS("no batt, assert prochot");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			}else if((total_W / 1000) <= (adapter_wattage *90/100)) {
				CPRINTS("no batt, de-assert prochot");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			}
		} else {
			if((total_W / 1000) > (adapter_wattage + battery_design_wattage)) {
				CPRINTS("assert prochot3");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 0);
			}else if((total_W / 1000) < (adapter_wattage + (battery_design_wattage*90/100))) {
				CPRINTS("de-assert prochot3");
				gpio_set_level(GPIO_EC_PROCHOT_ODL, 1);
			}
		}
	}
}
DECLARE_HOOK(HOOK_TICK, assert_prochot, HOOK_PRIO_DEFAULT);
