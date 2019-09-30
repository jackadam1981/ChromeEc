/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Power status reporting
 *
 * In order to use this functionality, the board must:
 *
 * 1) Provide a
 *	  const struct board_power_config * board_get_power_config(void);
 *    function which provides the board-level properties defined in the
 *    structure.
 *
 * 2) Define CONFIG_POWER_STATUS in your board.h
 *
 * 3) If the board's battery supports DBPTv2+, fill out the DBPT items
 *    in the struct board_power_config and add
 *    CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS to your board.h
 */

#include <string.h>

#include "battery.h"
#include "battery_common.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "power_status.h"
#include "usb_common.h"
#include "usb_pd.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/* DBPT register addresses */
#define DBPT_MAX_PEAK_POWER_ADDR 0x59
#define DBPT_SUS_PEAK_POWER_ADDR 0x5A
#define DBPT_SYS_RESISTANCE_ADDR 0x5C
#define DBPT_MIN_SYS_VOLTAGE_ADDR 0x5D

/* True if successfully able to program SysResistance and MinSysVoltage */
static bool dbpt_available;

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/* Current derated battery 1C level */
static int batt_1C_derated;

/* Outputs from DBPT algorithm */
static int batt_max_peak_power;
static int batt_sus_peak_power;

/***************************************************************
 * Get updated DBPT outputs every second.
 */
static void update_dbpt(void)
{
	if (!dbpt_available)
		return;

	if (sb_read(DBPT_MAX_PEAK_POWER_ADDR, &batt_max_peak_power) ||
	    sb_read(DBPT_SUS_PEAK_POWER_ADDR, &batt_sus_peak_power)) {
		batt_max_peak_power = 0;
		batt_sus_peak_power = 0;
	} else {
		/*
		 * The DBPT outputs are transferred as signed 16-bit
		 * values, and expected to be negative when the battery
		 * is discharging, so they are converted to positive
		 * Watts.
		 */
		batt_max_peak_power = (-(int16_t)batt_max_peak_power / 100);
		batt_sus_peak_power = (-(int16_t)batt_sus_peak_power / 100);
	}
}
#ifdef CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS
DECLARE_HOOK(HOOK_SECOND, update_dbpt, HOOK_PRIO_DEFAULT);
#endif /* CONFIG_BATTERY_SUPPORTS_DBPT2 */

/***************************************************************
 * Program initial DBPT parameters into the smart battery.
 */
static void program_dbpt_params(void)
{
	const struct board_power_config *config = board_get_power_config();
	int rv;

	/* Write SysResistance register */
	rv = sb_write(DBPT_SYS_RESISTANCE_ADDR, config->sys_resistance);
	if (rv != EC_SUCCESS) {
		CPRINTS("Unable to program SysResistance, no DBPT support!"
			"(%d)", rv);
		dbpt_available = false;
		return;
	}

	/* Write MinSysVoltage register */
	rv = sb_write(DBPT_MIN_SYS_VOLTAGE_ADDR, config->min_sys_voltage);
	if (rv != EC_SUCCESS) {
		CPRINTS("Unable to program MinSysVoltage, no DBPT support!"
			"(%d)", rv);
		dbpt_available = false;
		return;
	}

	CPRINTS("Battery supports DBPT, programmed");

	dbpt_available = true;
	update_dbpt();
}

/***************************************************************
 * Prints power information to console.
 */
static int dump_power_status(int argc, char **argv)
{
	const unsigned int ac_power = charge_manager_get_power_limit_uw() /
		1000000;

	CPRINTF("Current power source:\t");

	switch (current_power_source) {
	case POWER_SOURCE_BATTERY:
		CPRINTF("battery\n");
		break;
	case POWER_SOURCE_AC:
		CPRINTF("AC\n");
		break;
	case POWER_SOURCE_AC_BATTERY:
		CPRINTF("AC+battery\n");
		break;
	case POWER_SOURCE_UNKNOWN:
		CPRINTF("unknown\n");
		break;
	}

	CPRINTF("Max AC Power: %d W\n", ac_power);
	if (dbpt_available) {
		CPRINTF("DBPT Available\n");
		CPRINTF("Max Peak Power: %d W\n", batt_max_peak_power);
		CPRINTF("Max Sus Power: %d W\n", batt_sus_peak_power);
	} else {
		CPRINTF("DBPT Not available\n");
		CPRINTF("Batt 1C (derated): %d\n", batt_1C_derated);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(dumppower, dump_power_status, NULL,
			"Dump power status");

/***************************************************************
 * Determine how the board is being powered.
 */
static void update_power_source(void)
{
	const enum system_power_source old_power_source = current_power_source;
	int batt_soc;
	bool on_battery;

	batt_soc = get_battery_soc();

	/* Determine new power source */
	on_battery = get_latest_power_source();

	/*
	 * Inform the AP when power sources change, or if the battery
	 * SoC is less than or equal to BATTERY_LEVEL_LOW.
	 */
	if ((old_power_source != current_power_source) ||
		(on_battery && batt_soc <= BATTERY_LEVEL_LOW))
		pd_send_host_event(PD_EVENT_POWER_CHANGE);
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
#ifdef CONFIG_EXTPOWER
DECLARE_HOOK(HOOK_AC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
#endif

static void power_status_init(void)
{
	const struct board_power_config *config = board_get_power_config();

	/*
	 * Default setting for battery 1C, derated (1Cd)
	 * This is a conservative setting, recommended by Intel and explained
	 * below.
	 *
	 * a) 0.8 is Battery EOL (end-of-life) Derating factor
	 *    As the battery is charge cycled, its ability to retain a full
	 *    charge is diminished.  80% is the industry standard degradation
	 *    point for replacing a battery.
	 *
	 * b) 0.86 is Battery SoC Derating factor.  This relates
	 *    to the battery voltage drooping during discharge, often
	 *    as much as 14%, which means less power is available due
	 *    to lower voltage.
	 *
	 * 1Cd = 1C * 0.8 * 0.86 = 1C * 0.69
	 *
	 * Note that a less conservative setting could be implemented in the
	 * future, using actual battery voltage and estimated EOL detection
	 * via the fuel gauage.
	 */
	batt_1C_derated = (int)config->batt_1c_level * 69 / 100;

	/* Get an initial idea of the power source(s) */
	update_power_source();

	/*
	 * If DBPT params are not programmed (!dbpt_available), AND
	 * the system has a battery, then attempt to program
	 * the DBPT parameters.
	 */
	if (IS_ENABLED(CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS) &&
	    (current_power_source == POWER_SOURCE_BATTERY ||
	     current_power_source == POWER_SOURCE_AC_BATTERY))
		program_dbpt_params();
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);

/***************************************************************
 * Host command to retrieve power info v1
 */
static enum ec_status host_command_power_info(
	struct host_cmd_handler_args *args)
{
	const unsigned int ac_power = charge_manager_get_power_limit_uw() /
		1000000;
	struct ec_response_power_info_v1 *r = args->response;
	const int batt_soc = get_battery_soc();
	int dbpt_level = 0;

	if (IS_ENABLED(CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS) && dbpt_available)
		dbpt_level = 2;

	/* Get static values */
	r->config = board_get_power_config();

	/* These values are dynamic */
	r->battery_1cd = batt_1C_derated;
	r->ac_adapter_100pct = ac_power;
	r->battery_soc = batt_soc;
	r->system_power_source = current_power_source;

	/* Intel-specific items */
	r->intel.batt_dbpt_support_level = dbpt_level;

	/* Fill the batt power values based on the battery config */
	if (IS_ENABLED(CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS)) {
		r->intel.batt_dbpt_max_peak_power = 0;
		r->intel.batt_dbpt_sus_peak_power = 0;
	} else {
		r->intel.batt_dbpt_max_peak_power = batt_max_peak_power;
		r->intel.batt_dbpt_sus_peak_power = batt_sus_peak_power;
	}

	/*
	 * If USB-PD is used, then this number is the same as the 100pct
	 * rating, otherwise the default is 1.5x the 100% rating.
	 */
#ifdef CONFIG_USB_POWER_DELIVERY
	r->ac_adapter_10ms = ac_power;
#else
	r->ac_adapter_10ms = ac_power * 3 / 2;
#endif

	args->response_size = sizeof(*r);
	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_POWER_INFO,
		host_command_power_info,
		EC_VER_MASK(1));
