/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Support Power Participant in DTT(Dynamic Tuning Technology) */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "throttle_ap.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_SYSTEM, "DTT:" format, ##args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ##args)

/* Maximum limit in PSRC[7:4] - sequence number */
#define PSRC_SEQUENCE_MAX 16

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/*
 * Deassert PROCHOT, if already asserted.
 * PROCHOT requires to be deasserted either based on Power boss OK ACK
 * or within 2 seconds of no recieval of Power boss OK ACK.
 */
static void deassert_prochot(void)
{
	/* deassert PROCHOT */
	throttle_ap(THROTTLE_OFF, THROTTLE_HARD, THROTTLE_SRC_DTT);
}
DECLARE_DEFERRED(deassert_prochot);

/*
 * Function evaluates the current source of power and then returns true,
 * if the power source is either on battery or on both battery & AC.
 */
bool is_latest_power_source_with_battery(void)
{
	const bool batt_pres = battery_is_present() == BP_YES;
	const int ext_pres = extpower_is_present();

	/* Determine new power source */
	if (ext_pres && batt_pres) {
		current_power_source = POWER_SOURCE_AC_BATTERY;
		CPRINTS("Current Power Source: A/C and battery");
	} else if (ext_pres) {
		current_power_source = POWER_SOURCE_AC;
		CPRINTS("Current Power Source: A/C");
	} else if (batt_pres) {
		current_power_source = POWER_SOURCE_BATTERY;
		CPRINTS("Current Power Source: battery");
	} else {
		CPRINTS("Power glitch encountered, no power source!");
		current_power_source = POWER_SOURCE_UNKNOWN;
	}

	if (current_power_source == POWER_SOURCE_BATTERY)
		return true;
	else
		return false;
}

/* Update the source of platform power */
static void update_power_source(void)
{
	const enum system_power_source prev_power_source = current_power_source;
	/* power delivery state change sequence number in PSRC[7:4] */
	static int psrc_sequence_num;
	int power_src;

	int new_charge_current;
	static int prev_charge_current;

	/* Flag to ensure whether low battery case is already reported */
	static bool low_batt_reported_flag;

	static int prev_charge_port = CHARGE_PORT_NONE;

	int current_port = charge_manager_get_active_charge_port();

	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);

	/* Get the percentage of state of charge */
	int batt_soc = usb_get_battery_soc();

	/* Check whether the source of power is with battery */
	bool on_battery = is_latest_power_source_with_battery();

	if (current_port == CHARGE_PORT_NONE)
		new_charge_current = prev_charge_current = 0;
	else
		new_charge_current = charge_manager_get_charger_current();

	/*
	 * Return in any of the following case:
	 * a) When there is no power source change.
	 * b) When there is no port change in the case of AC
	 * c) When there is no charge current change
	 * d) When battery state of charge is greater than BATTERY_LEVEL_LOW
	 * Otherwise progress to inform AP.
	 */
	if ((current_port == prev_charge_port) &&
	    (new_charge_current == prev_charge_current) &&
	    (current_power_source != prev_power_source) &&
	    (!on_battery || batt_soc > BATTERY_LEVEL_LOW)) {
		return;
	}

	/* AC removal or low battery */
	if ((current_power_source == POWER_SOURCE_BATTERY) ||
	    (current_port == CHARGE_PORT_NONE)) {
		/* return, if low battery is already reported */
		if (low_batt_reported_flag)
			return;

		/* DC source */
		power_src = SOURCE_DC;

		if (current_port == CHARGE_PORT_NONE)
			current_power_source = POWER_SOURCE_BATTERY;

		/*
		 * Increment sequence number and take care not to
		 * overflow.
		 */
		psrc_sequence_num = (psrc_sequence_num + 1) % PSRC_SEQUENCE_MAX;

		/* Throttle action */
		throttle_ap(THROTTLE_ON, THROTTLE_HARD, THROTTLE_SRC_DTT);

		/* Deassert PROCHOT, if not done within 2 seconds */
		hook_call_deferred(&deassert_prochot_data, 2 * SECOND);

		prev_charge_port = CHARGE_PORT_NONE;
		new_charge_current = prev_charge_current = 0;

		/*
		 * Flag to indicate that low battery case need not be
		 * reported further.
		 */
		if (batt_soc <= BATTERY_LEVEL_LOW)
			low_batt_reported_flag = true;
	} else {
		/* TODO(b:205928013): ADD barrel jack case */

		/* AC Source is USBC */
		power_src = SOURCE_AC_USBC;

		prev_charge_port = current_port;
		prev_charge_current = new_charge_current;

		/* If low battery is reported previously, clear the flag */
		if (low_batt_reported_flag)
			low_batt_reported_flag = false;
	}
	*memmap_psrc = (power_src | (psrc_sequence_num << 4));

	/* Send SCI Event */
	pd_send_host_event(PD_EVENT_POWER_CHANGE);
}
DECLARE_HOOK(HOOK_BATTERY_SOC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_POWER_SUPPLY_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_EXTPOWER
DECLARE_HOOK(HOOK_AC_CHANGE, update_power_source, HOOK_PRIO_DEFAULT);
#endif

static void power_status_init(void)
{
	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);

	/* Initial Value */
	*memmap_psrc = 0;

	/* Update the initial information on power source */
	update_power_source();
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);
