/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_smart.h"
#include "bd9995x.h"
#include "charge_ramp.h"
#include "charge_state.h"
#include "charger_profile.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "i2c.h"
#include "util.h"

enum battery_type {
	BATTERY_SONY_CORP,
	BATTERY_SMP_COS4870,
	BATTERY_SMP_C22N1626,
	BATTERY_CPT_C22N1626,
	BATTERY_TYPE_COUNT,
};

struct board_batt_params {
	char *manuf_name;
	int ship_mode_reg;
	int ship_mode_data;
	struct battery_info batt_info;
	int (*batt_init)(void);
};

#define DEFAULT_BATTERY_TYPE BATTERY_SONY_CORP

static enum battery_present batt_pres_prev = BP_NOT_SURE;

static enum battery_type board_battery_type = BATTERY_TYPE_COUNT;

static int batt_smp_cos4870_init(void)
{
	int batt_status;

	return battery_status(&batt_status) ? 0 :
		batt_status & STATUS_INITIALIZED;
}

static int batt_sony_corp_init(void)
{
	/* TODO: crosbug.com/p/59904 */
	return 1;
}

static const struct board_batt_params info[] = {
	/* SONY CORP BATTERY battery specific configurations */
	[BATTERY_SONY_CORP] = {
		.manuf_name = "SONYCorp",
		.ship_mode_reg = 0x3A,
		.ship_mode_data = 0xC574,
		.batt_init = batt_sony_corp_init,

		/* Battery info for BQ40z555 (TODO: crosbug.com/p/59904) */
		.batt_info = {
			.voltage_max = 8700,	/* mV */
			.voltage_normal = 7600,

			/*
			 * Actual value 6000mV, added 100mV for charger accuracy
			 * so that unwanted low VSYS_Prochot# assertion can be
			 * avoided.
			 */
			.voltage_min = 6100,
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 46,
			.charging_min_c = 0,
			.charging_max_c = 45,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},

	/* SMP COS4870 BATTERY battery specific configurations */
	[BATTERY_SMP_COS4870] = {
		.manuf_name = "SMP-COS4870",
		.ship_mode_reg = 0x00,
		.ship_mode_data = 0x0010,
		.batt_init = batt_smp_cos4870_init,

		/* Battery info for BQ40Z55 */
		.batt_info = {
			.voltage_max = 8700,	/* mV */
			.voltage_normal = 7600,

			/*
			 * Actual value 6000mV, added 100mV for charger accuracy
			 * so that unwanted low VSYS_Prochot# assertion can be
			 * avoided.
			 */
			.voltage_min = 6100,
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 46,
			.charging_min_c = 0,
			.charging_max_c = 45,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
	/* SMP C22N1626 BATTERY battery specific configurations */
	[BATTERY_SMP_C22N1626] = {
		.manuf_name = "AS1FNZD3KD",
		.ship_mode_reg = 0x00,
		.ship_mode_data = 0x0010,
		.batt_init = batt_smp_cos4870_init,

		/* Battery info for BQ40Z55 */
		.batt_info = {
			.voltage_max = 8800,	/* mV */
			.voltage_normal = 7700,

			/*
			 * Actual value 6000mV, added 100mV for charger accuracy
			 * so that unwanted low VSYS_Prochot# assertion can be
			 * avoided.
			 */
			.voltage_min = 6100,
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.charging_max_c = 60,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
	/* CPT C22N1626 BATTERY battery specific configurations */
	[BATTERY_CPT_C22N1626] = {
		.manuf_name = "AS1FOAD3KD",
		.ship_mode_reg = 0x00,
		.ship_mode_data = 0x0010,
		.batt_init = batt_smp_cos4870_init,

		/* Battery info for BQ40Z55 */
		.batt_info = {
			.voltage_max = 8800,	/* mV */
			.voltage_normal = 7700,

			/*
			 * Actual value 6000mV, added 100mV for charger accuracy
			 * so that unwanted low VSYS_Prochot# assertion can be
			 * avoided.
			 */
			.voltage_min = 6100,
			.precharge_current = 256,	/* mA */
			.start_charging_min_c = 0,
			.start_charging_max_c = 45,
			.charging_min_c = 0,
			.charging_max_c = 60,
			.discharging_min_c = 0,
			.discharging_max_c = 60,
		},
	},
};
BUILD_ASSERT(ARRAY_SIZE(info) == BATTERY_TYPE_COUNT);

static inline const struct board_batt_params *board_get_batt_params(void)
{
	return &info[board_battery_type == BATTERY_TYPE_COUNT ?
			DEFAULT_BATTERY_TYPE : board_battery_type];
}

static inline enum battery_present battery_hw_present(void)
{
	/* The GPIO is low when the battery is physically present */
	return gpio_get_level(GPIO_EC_BATT_PRES_L) ? BP_NO : BP_YES;
}

/* Get type of the battery connected on the board */
static int board_get_battery_type(void)
{
	char name[32];
	int i;

	if (!battery_manufacturer_name(name, sizeof(name))) {
		for (i = 0; i < BATTERY_TYPE_COUNT; i++) {
			if (!strcasecmp(name, info[i].manuf_name)) {
				board_battery_type = i;
				break;
			}
		}
	}

	return board_battery_type;
}

/*
 * Initialize the battery type for the board.
 *
 * Very first battery info is called by the charger driver to initialize
 * the charger parameters hence initialize the battery type for the board
 * as soon as the I2C is initialized.
 */
static void board_init_battery_type(void)
{
	board_get_battery_type();
}
DECLARE_HOOK(HOOK_INIT, board_init_battery_type, HOOK_PRIO_INIT_I2C + 1);

const struct battery_info *battery_get_info(void)
{
	return &board_get_batt_params()->batt_info;
}

int board_cut_off_battery(void)
{
	int rv;
	const struct board_batt_params *board_battery = board_get_batt_params();

	/* Ship mode command must be sent twice to take effect */
	rv = sb_write(board_battery->ship_mode_reg,
			board_battery->ship_mode_data);
	if (rv != EC_SUCCESS)
		return rv;

	return sb_write(board_battery->ship_mode_reg,
			board_battery->ship_mode_data);
}

enum battery_disconnect_state battery_get_disconnect_state(void)
{
	uint8_t data[6];
	int rv;

	/*
	 * Take note if we find that the battery isn't in disconnect state,
	 * and always return NOT_DISCONNECTED without probing the battery.
	 * This assumes the battery will not go to disconnect state during
	 * runtime.
	 */
	static int not_disconnected;

	if (not_disconnected)
		return BATTERY_NOT_DISCONNECTED;

	if (extpower_is_present()) {
		/* Check if battery charging + discharging is disabled. */
		rv = sb_write(SB_MANUFACTURER_ACCESS,
			      PARAM_OPERATION_STATUS);
		if (rv)
			return BATTERY_DISCONNECT_ERROR;

		rv = sb_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
				    SB_ALT_MANUFACTURER_ACCESS, data, 6);

		if (rv || (~data[3] & (BATTERY_DISCHARGING_DISABLED |
				       BATTERY_CHARGING_DISABLED))) {
			not_disconnected = 1;
			return BATTERY_NOT_DISCONNECTED;
		}

		/*
		 * Battery is neither charging nor discharging. Verify that
		 * we didn't enter this state due to a safety fault.
		 */
		rv = sb_write(SB_MANUFACTURER_ACCESS, PARAM_SAFETY_STATUS);
		if (rv)
			return BATTERY_DISCONNECT_ERROR;

		rv = sb_read_string(I2C_PORT_BATTERY, BATTERY_ADDR,
				    SB_ALT_MANUFACTURER_ACCESS, data, 6);

		if (rv || data[2] || data[3] || data[4] || data[5])
			return BATTERY_DISCONNECT_ERROR;

		/*
		 * Battery is present and also the status is initialized and
		 * no safety fault, battery is disconnected.
		 */
		if (battery_is_present() == BP_YES)
			return BATTERY_DISCONNECTED;
	}
	not_disconnected = 1;
	return BATTERY_NOT_DISCONNECTED;
}

#ifdef CONFIG_CHARGER_PROFILE_OVERRIDE
const struct fast_charge_profile fast_charge_info[] = {
	/* < 0C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(-1),
		.curr_high_vtg = 0,
		.curr_low_vtg = 0,
		.voltage = 0,
	},

	/* 0C >= && <=15C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(15),
		.curr_high_vtg = 472,
		.curr_low_vtg = 944,
		.voltage = 8700,
	},

	/* 15C > && <=20C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(20),
		.curr_high_vtg = 1416,
		.curr_low_vtg = 1416,
		.voltage = 8700,
	},

	/* 20C > && <=45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(45),
		.curr_high_vtg = 3300,
		.curr_low_vtg = 3300,
		.voltage = 8700,
	},

	/* > 45C */
	{
		.temp_c = TEMPC_FLOAT_TO_INT(0xFFFF),
		.curr_high_vtg = 0,
		.curr_low_vtg = 0,
		.voltage = 0,
	},
};

struct fast_charge_config fast_charge_config_info = {
	.temp_ranges = ARRAY_SIZE(fast_charge_info),
	.temp_last_range = 2,
	.vtg_low_limit = 8000,
	.vtg_high_limit = 8000,
};

static int charger_should_discharge_on_ac(struct charge_state_data *curr)
{
	/* can not discharge on AC without battery */
	if (curr->batt.is_present != BP_YES)
		return 0;

	/* Do not discharge on AC if the battery is still waking up */
	if (!(curr->batt.flags & BATT_FLAG_WANT_CHARGE) &&
		!(curr->batt.status & STATUS_FULLY_CHARGED))
		return 0;

	/*
	 * In light load (<450mA being withdrawn from VSYS) the DCDC of the
	 * charger operates intermittently i.e. DCDC switches continuously
	 * and then stops to regulate the output voltage and current, and
	 * sometimes to prevent reverse current from flowing to the input.
	 * This causes a slight voltage ripple on VSYS that falls in the
	 * audible noise frequency (single digit kHz range). This small
	 * ripple generates audible noise in the output ceramic capacitors
	 * (caps on VSYS and any input of DCDC under VSYS).
	 *
	 * To overcome this issue enable the battery learning operation
	 * and suspend USB charging and DC/DC converter.
	 */
	if (!battery_is_cut_off() &&
		!(curr->batt.flags & BATT_FLAG_WANT_CHARGE) &&
		(curr->batt.status & STATUS_FULLY_CHARGED))
		return 1;

	/*
	 * To avoid inrush current from the external charger, enable
	 * discharge on AC till the new charger is detected and charge
	 * detect delay has passed.
	 */
	if (!chg_ramp_is_detected() && curr->batt.state_of_charge > 2)
		return 1;

	return 0;
}

/*
 * This can override the smart battery's charging profile. To make a change,
 * modify one or more of requested_voltage, requested_current, or state.
 * Leave everything else unchanged.
 *
 * Return the next poll period in usec, or zero to use the default (which is
 * state dependent).
 */

/* TODO: crosbug.com/p/59904 */
int charger_profile_override(struct charge_state_data *curr)
{
	int disch_on_ac = charger_should_discharge_on_ac(curr);

	charger_discharge_on_ac(disch_on_ac);

	if (disch_on_ac) {
		curr->state = ST_DISCHARGE;
		return 0;
	}

	return charger_profile_override_common(curr,
						NULL,//fast_charge_info,
						&fast_charge_config_info);
}
#endif /* CONFIG_CHARGER_PROFILE_OVERRIDE */

/*
 * Physical detection of battery.
 */
enum battery_present battery_is_present(void)
{
	enum battery_present batt_pres;

	/* Get the physical hardware status */
	batt_pres = battery_hw_present();

	/*
	 * Make sure battery status is implemented, I2C transactions are
	 * success & the battery status is Initialized to find out if it
	 * is a working battery and it is not in the cut-off mode.
	 *
	 * If battery I2C fails but VBATT is high, battery is booting from
	 * cut-off mode.
	 *
	 * FETs are turned off after Power Shutdown time.
	 * The device will wake up when a voltage is applied to PACK.
	 * Battery status will be inactive until it is initialized.
	 */
	if (batt_pres == BP_YES && batt_pres_prev != batt_pres &&
		!battery_is_cut_off()) {
		/* Re-init board battery if battery presence status changes */
		if (board_get_battery_type() == BATTERY_TYPE_COUNT) {
			if (bd9995x_get_battery_voltage() >=
			    board_get_batt_params()->batt_info.voltage_min)
				batt_pres = BP_NO;
		} else if (!board_get_batt_params()->batt_init())
			batt_pres = BP_NO;
	}

	batt_pres_prev = batt_pres;

	return batt_pres;
}

int board_battery_initialized(void)
{
	return battery_hw_present() == batt_pres_prev;
}
