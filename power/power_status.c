/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Power status reporting
 *
 * In order to use this module, the board must provide a
 *
 *   const struct board_power_config * board_get_power_config(void);
 *
 * function which provides the board-level properties defined in the
 * structure.
 *
 * Also define CONFIG_POWER_STATUS in your board.h
 *
 * If the board's battery supports DBPTv2+, please fill out the DBPT
 * items in the struct board_power_config and add
 * CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS to your board.h
 */

#include <string.h>

#include "battery.h"
#include "board_config.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "ec_commands.h"
#include "power.h"
#include "system.h"
#include "hooks.h"
#include "extpower.h"
#include "charge_manager.h"
#include "power_status.h"
#include "battery_smart.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_CHIPSET, format, ## args)

#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
/* DBTF registers */
#define DBTF_MAX_PEAK_POWER_ADDR 0x59
#define DBTF_SUS_PEAK_POWER_ADDR 0x5A
#define DBTF_SYS_RESISTANCE_ADDR 0x5C
#define DBTF_MIN_SYS_VOLTAGE_ADDR 0x5D
/* True if successfully able to program SysResistance and MinSysVoltage */
static bool dbpt_available;
#endif /* CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS */

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/* Current derated battery 1C level */
static int batt_1C_derated;
static int batt_max_peak_power;
static int batt_sus_peak_power;

/***************************************************************
 * Program initial DBPT parameters into the smart battery
 */
#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
static void program_dbpt_params(void)
{
	const struct board_power_config *config = board_get_power_config();
	int rv;

	/* Write SysResistance register */
	rv = sb_write(DBTF_SYS_RESISTANCE_ADDR, config->sys_resistance);
	if (rv != EC_SUCCESS) {
		CPRINTS("Unable to program SysResistance, no DBPT support!"
			"(%d)", rv);
		dbpt_available = false;
		return;
	}

	/* Write MinSysVoltage register */
	rv = sb_write(DBTF_MIN_SYS_VOLTAGE_ADDR, config->min_sys_voltage);
	if (rv != EC_SUCCESS) {
		CPRINTS("Unable to program MinSysVoltage, no DBPT support!"
			"(%d)", rv);
		dbpt_available = false;
		return;
	}

	CPRINTS("Battery supports DBPT, programmed");
	dbpt_available = true;
}
#endif /* #ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS */

static int get_battery_soc(void)
{
	int soc = 0;

#if defined(CONFIG_CHARGER)
	soc = charge_get_percent();
#elif defined(CONFIG_BATTERY)
	soc = board_get_battery_soc();
#endif

	return soc;
}

static int dump_power_status(int argc, char **argv)
{
	const unsigned int ac_power = charge_manager_get_power_limit_uw() /
		1000000;

	CPRINTF("Current power source:");

	switch (current_power_source) {
	case POWER_SOURCE_BATTERY:
		CPRINTF("\tbattery\n");
		break;
	case POWER_SOURCE_AC:
		CPRINTF("\tAC\n");
		break;
	case POWER_SOURCE_AC_BATTERY:
		CPRINTF("\tAC+battery\n");
		break;
	case POWER_SOURCE_UNKNOWN:
		CPRINTF("\tunknown\n");
		break;
	}

	CPRINTF("AC Power: %d W\n", ac_power);
#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
	if (dbpt_available) {
		CPRINTF("DBPT Available\n");
		CPRINTF("Max Peak Power: %d W\n", batt_max_peak_power);
		CPRINTF("Max Sus Power: %d W\n", batt_sus_peak_power);
	} else {
		CPRINTF("DBPT Not available\n");
	}
#else
	CPRINTF("DBPT Not available\n");
	CPRINTF("Batt 1C (derated): %d\n",
		board_get_power_config()->batt_1C_level * 69 / 100);
#endif
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dumppower, dump_power_status, NULL,
			"Dump power status");

/* Determine how the board is being powered */
static void update_power_source(void)
{
	const bool batt_pres = battery_is_present() == BP_YES;
	const int ext_pres = extpower_is_present();
	const enum system_power_source old_power_source = current_power_source;
	bool glitch = false;
#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
	bool on_battery;
	bool was_not_on_battery;
#endif

	switch (current_power_source) {
	case POWER_SOURCE_BATTERY:
		if (ext_pres)
			current_power_source = POWER_SOURCE_AC_BATTERY;

		if (!batt_pres)
			glitch = true;
		break;

	case POWER_SOURCE_AC:
		if (batt_pres)
			current_power_source = POWER_SOURCE_AC_BATTERY;

		if (!ext_pres)
			glitch = true;
		break;

	case POWER_SOURCE_AC_BATTERY:
		if (!batt_pres) {
			current_power_source = POWER_SOURCE_AC;
#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
			/* Battery went away, so DBPT is not available */
			dbpt_available = false;
#endif /* CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS */
		}
		if (!ext_pres)
			current_power_source = POWER_SOURCE_BATTERY;

		if (!batt_pres && !ext_pres)
			glitch = true;
		break;

	case POWER_SOURCE_UNKNOWN:
		if (batt_pres && ext_pres)
			current_power_source = POWER_SOURCE_AC_BATTERY;
		else if (batt_pres)
			current_power_source = POWER_SOURCE_BATTERY;
		else if (ext_pres)
			current_power_source = POWER_SOURCE_AC;
	}

	if (glitch) {
		current_power_source = POWER_SOURCE_UNKNOWN;
		CPRINTS("Power glitch encountered, no power source!");
	}

#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
	/*
	 * If DBPT params are not programmed, AND the system just found a
	 * battery, then attempt to program the DBPT parameters.
	 */
	on_battery = (current_power_source == POWER_SOURCE_BATTERY ||
		      current_power_source == POWER_SOURCE_AC_BATTERY);
	was_not_on_battery = (old_power_source == POWER_SOURCE_UNKNOWN ||
			      old_power_source == POWER_SOURCE_AC);

	if (!dbpt_available && on_battery && was_not_on_battery)
		program_dbpt_params();

#endif /* CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS */

	/*
	 * If the power source changed, inform the AP so new power limits
	 * can be calculated.
	 */
	if (old_power_source != current_power_source)
		host_set_single_event(EC_HOST_EVENT_POWER_CHANGE);
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_INIT, update_power_source, HOOK_PRIO_LAST);
#ifdef CONFIG_EXTPOWER
DECLARE_HOOK(HOOK_AC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
#endif

#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
static void update_dbpt(void)
{
	if (dbpt_available) {
		if (sb_read(DBTF_MAX_PEAK_POWER_ADDR, &batt_max_peak_power) ||
		    sb_read(DBTF_SUS_PEAK_POWER_ADDR, &batt_sus_peak_power)) {
			batt_max_peak_power = 0;
			batt_sus_peak_power = 0;
		} else {
			/*
			 * The DBPT outputs are transferred as signed 16-bit
			 * values, and expected to be negative when the battery
			 * is discharging, so they are converted to positive
			 * Watts.
			 */
			batt_max_peak_power =
				(-(int16_t)batt_max_peak_power / 100);
			batt_sus_peak_power =
				(-(int16_t)batt_sus_peak_power / 100);
		}
	}
}
DECLARE_HOOK(HOOK_SECOND, update_dbpt, HOOK_PRIO_DEFAULT);
#endif /* CONFIG_BATTERY_SUPPORTS_DBPT2 */

/*
 * When input power changes, set the POWER_CHANGE event bit so that the AP
 * will gather the power status information.
 */
static void alert_ap_input_power_changed(void)
{
	host_set_single_event(EC_HOST_EVENT_POWER_CHANGE);
}
DECLARE_HOOK(HOOK_INPUT_POWER_CHANGED, alert_ap_input_power_changed,
	     HOOK_PRIO_FIRST);

/***************************************************************
 * Host command to retrieve AP power status
 */
static int host_command_get_power_status(struct host_cmd_handler_args *args)
{
	const struct ec_params_get_power_status *p = args->params;
	struct ec_response_get_power_status *r = args->response;
	const unsigned int ac_power = charge_manager_get_power_limit_uw() /
		1000000;
	int batt_soc = get_battery_soc();
	int dbpt_level = 0;
	const struct board_power_config *config = board_get_power_config();

#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
	if (dbpt_available)
		dbpt_level = 2;
#endif

	r->battery_soc = batt_soc;
	r->system_power_source = current_power_source;

	/* 1C * 0.8 * 0.86 = 1C * 0.69 */
	batt_1C_derated = config->batt_1c_level * 69 / 100;

	switch (p->power_status_type) {
	case POWER_STATUS_BASE:
		r->base.nominal_charger_eff = config->nominal_charger_eff;
		r->base.rop_avg_eff = config->rop_avg_eff;
		r->base.rop_peak_eff = config->rop_peak_eff;
		r->base.soc_avg_eff = config->soc_avg_eff;
		r->base.soc_peak_eff = config->soc_peak_eff;
		r->base.batt_dbpt_support_level = dbpt_level;
		break;

	case POWER_STATUS_DYNAMIC:
		r->dynamic.ac_adapter_100pct = ac_power;
		/*
		 * If USB-PD is used, then this number is the same as the 100pct
		 * rating, otherwise the default is 1.5x the 100% rating.
		 */
#ifdef CONFIG_USB_POWER_DELIVERY
		r->dynamic.ac_adapter_10ms = ac_power;
#else
		/* barrel-jack charger, not PD */
		r->dynamic.ac_adapter_10ms = ac_power * 3 / 2;
#endif
		/* x1.38 - from Intel */
		r->dynamic.battery_2cd = (uint8_t)(batt_1C_derated * 69 / 50);
		/* x2.75 - from Intel */
		r->dynamic.battery_4cd = (uint8_t)(batt_1C_derated * 11 / 4);
		r->dynamic.batt_dbpt_max_peak_power =
			(uint8_t)batt_max_peak_power;
		r->dynamic.batt_dbpt_sus_peak_power =
			(uint8_t)batt_sus_peak_power;
		r->dynamic.rop_avg = config->rop_avg;
		r->dynamic.rop_peak = config->rop_peak;
		break;
	}

	args->response_size = sizeof(*r);
	return EC_SUCCESS;
}

DECLARE_HOST_COMMAND(EC_CMD_GET_POWER_STATUS,
		     host_command_get_power_status,
		     EC_VER_MASK(0));
