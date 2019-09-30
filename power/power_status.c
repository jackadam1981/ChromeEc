/* Copyright 2022 The Chromium OS Authors. All rights reserved.
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
#include "battery_smart.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "console.h"
#include "chipset.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "power_status.h"
#include "throttle_ap.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/*
 * TODO: Fill up with actual threshold values of
 * batt_max_peak_power and batt_sus_peak_power
 */
#define MAX_PEAK_THRESHOLD_W 5
#define SUS_PEAK_THRESHOLD_W 5

/* True if successfully able to program SysResistance and MinSysVoltage */
static bool dbpt_available;

/* Current system power source */
static enum ec_system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/* Current derated battery 1C level */
static int batt_1C_derated;

/* Outputs from DBPT algorithm */
static int batt_max_peak_power;
static int batt_sus_peak_power;

/* PD state change sequence number */
static int pd_state_sequence;

/* PROCHOT action based on PD state change sequence number */
static int prochot_action = PROCHOT_DEASSERT_OK;

/* pd_state_sequence is 8 bit number */
#define PD_STATE_SEQUENCE_MAX 15

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

		/* Assert interrupt to AP when batt_max_peak_power or
		 * batt_sus_peak_power reaches threshold
		 */
		if (batt_max_peak_power >= MAX_PEAK_THRESHOLD_W ||
			batt_sus_peak_power >= SUS_PEAK_THRESHOLD_W)
			pd_send_host_event(PD_EVENT_POWER_CHANGE);

		/*
		 * Write DBPT outputs to memory mapped region to be accessed by
		 * kernel through ACPI tables
		 */
		*host_get_memmap(EC_MEMMAP_BATT_PMAX) = batt_max_peak_power;
		*host_get_memmap(EC_MEMMAP_BATT_PBSS) = batt_sus_peak_power;
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

	current_power_source = POWER_SOURCE_AC;
	if (current_power_source & POWER_SOURCE_BATTERY)
		CPRINTF("battery\n");
	else if (current_power_source & POWER_SOURCE_AC)
		CPRINTF("AC\n");
	else if (current_power_source & POWER_SOURCE_ALL)
		CPRINTF("AC+BAT0+BAT1\n");
	else
		CPRINTF("unknown power source\n");

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
 * Deassert PROCHOT when PBOK response is received
 */
static void deassert_prochot(void)
{
	if (prochot_action == PROCHOT_DEASSERT_OK)
		throttle_ap(THROTTLE_OFF, THROTTLE_HARD,
			THROTTLE_SRC_BAT_DISCHG_CURRENT);
}
DECLARE_DEFERRED(deassert_prochot);

#ifdef CONFIG_CHARGE_MANAGER
/***************************************************************
 * Increment Power delivery status change sequence number.
 * Whenever USB PD connects or disconnects, power delivery
 * sequence number is incremented to notify Intel DTT. DTT can
 * adjust power limits and sends PBOK with the corresponding
 * sequence number to EC. This is to synchronize the operation
 * between EC and DTT.
 */
void increment_pd_seq(void)
{
	int chg_port;
	/* Check if the charger port has been disconnected */
	chg_port = charge_manager_get_active_charge_port();
	if (pd_is_disconnected(chg_port)) {
		/*
		 * pd_state_sequence is 8 bit number, wrapping it
		 * around if it overflows
		 */
		pd_state_sequence = pd_state_sequence == PD_STATE_SEQUENCE_MAX
			? 0 : pd_state_sequence + 1;
	}
}
#endif

/***************************************************************
 * Determine how the board is being powered.
 */
static void update_power_source(void)
{
	const enum ec_system_power_source old_power_source =
				current_power_source;
	int batt_soc;
	bool on_battery;

	if (IS_ENABLED(CONFIG_USB_POWER_DELIVERY))
		batt_soc = usb_get_battery_soc();

	/* Determine new power source */
	on_battery = get_latest_power_source();

	/*
	 * Inform the AP when power sources change, or if the battery
	 * SoC is less than or equal to BATTERY_LEVEL_LOW.
	 */
	if ((old_power_source != current_power_source) ||
		(on_battery && batt_soc <= BATTERY_LEVEL_LOW)) {

		#ifdef CONFIG_CHARGE_MANAGER
		increment_pd_seq();
		#endif

		pd_send_host_event(PD_EVENT_POWER_CHANGE);
		throttle_ap(THROTTLE_ON, THROTTLE_HARD,
					THROTTLE_SRC_BAT_DISCHG_CURRENT);
		hook_call_deferred(&deassert_prochot_data, 2 * SECOND);
	}
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_USB_PD_DISCONNECT, update_power_source, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_USB_PD_CONNECT, update_power_source, HOOK_PRIO_DEFAULT);

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
	     current_power_source == POWER_SOURCE_ALL))
		program_dbpt_params();
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);

/***************************************************************
 * Host command to retrieve power info v1
 */
static enum ec_status host_command_power_info(
	struct host_cmd_handler_args *args)
{
	const struct board_power_config *config = board_get_power_config();
	const unsigned int ac_power = charge_manager_get_power_limit_uw() /
		1000000;
	struct ec_response_power_info_v1 *r = args->response;
	const int batt_soc = usb_get_battery_soc();
	int dbpt_level = 0;

	if (IS_ENABLED(CONFIG_BATTERY_SUPPORTS_DBPT_V2PLUS) && dbpt_available)
		dbpt_level = 2;

	/* Get static values */
	r->nominal_charger_eff = config->nominal_charger_eff;
	r->nominal_charger_eff = 5;
	r->rop_avg_eff = config->rop_avg_eff;
	r->rop_peak_eff = config->rop_peak_eff;
	r->soc_avg_eff = config->soc_avg_eff;
	r->soc_peak_eff = config->soc_peak_eff;
	r->rop_avg = config->rop_avg;
	r->rop_peak = config->rop_peak;

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
	if (IS_ENABLED(CONFIG_USB_POWER_DELIVERY))
		r->ac_adapter_10ms = ac_power;
	else
		r->ac_adapter_10ms = ac_power * 3 / 2;

	args->response_size = sizeof(*r);
	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_POWER_INFO,
		host_command_power_info,
		EC_VER_MASK(1));

/***************************************************************
 * Host command for Power Boss OK
 */
static enum ec_status host_command_power_boss_ok(
				struct host_cmd_handler_args *args)
{
	const struct ec_params_power_boss_ok *p = args->params;
	struct ec_response_power_boss_ok *r = args->response;

	/*
	 * The reason to have a power delivery state change sequence
	 * number is to synchronize the operation between EC and Intel
	 * DTT. Power delivery state could change more often than DTT can
	 * handle in time. Using this sequence number is to ensure
	 * that the PROCHOT that EC cleared is the one that DTT has processed
	 */
	if (pd_state_sequence != p->pd_sequence)
		prochot_action = PROCHOT_DEASSERT_NOT_OK;

	r->prochot_action = prochot_action;

	args->response_size = sizeof(*r);

	return EC_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_POWER_BOSS_OK, host_command_power_boss_ok,
						EC_VER_MASK(0));
