/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Support Power Participant in Dynamic Tuning Technology */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "dptf.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "throttle_ap.h"
#include "timer.h"
#include "usb_common.h"
#include "usb_pd.h"

/* Console output macros */
#define CPRINTS(format, args...) cprints(CC_SYSTEM, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SYSTEM, format, ## args)

/* pd_state_sequence number */
#define PD_STATE_SEQUENCE_MAX 16

/* pd_sequence corresponds to bit[0:3] in PSRC */
#define PD_SEQUENCE_MASK	0xF0

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/* calculate and return the adapter rating */
static int get_adapter_rating(void)
{
	int adapter_current_ma;
	int adapter_voltage_mv;
	int adapter_rating;

	adapter_current_ma = charge_manager_get_charger_current();
	adapter_voltage_mv = charge_manager_get_charger_voltage();
	adapter_rating = adapter_current_ma * adapter_voltage_mv / 1000;

	CPRINTS("ARTG - %d mW", adapter_rating);
	return adapter_rating;
}

/* Deasset PROCHOT, if already asserted.
 * PROCHOT requires to be deasserted either based on Power boss OK ACK
 * or within 2 seconds of no recieval of Power boss OK ACK.
 */
static void deassert_prochot(void)
{
	/* deassert PROCHOT */
	throttle_ap(THROTTLE_OFF, THROTTLE_HARD,
			THROTTLE_SRC_BAT_DISCHG_CURRENT);
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

	if ((current_power_source == POWER_SOURCE_BATTERY) ||
			(current_power_source == POWER_SOURCE_AC_BATTERY))
		return true;
	else
		return false;
}

/* Update the source of platform power */
static void update_power_source(void)
{
	const enum system_power_source prev_power_source =
					current_power_source;

	/* PD state change sequence number */
	static int pd_state_sequence;

	static int prev_charge_port = CHARGE_PORT_NONE;

	int current_port = charge_manager_get_active_charge_port();

	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);
	uint16_t *memmap_artg = (uint16_t *)host_get_memmap(EC_MEMMAP_PWR_ARTG);

	/* Get the percentage of state of charge */
	int batt_soc = usb_get_battery_soc();

	/* Check whether the source of power is with battery */
	bool on_battery = is_latest_power_source_with_battery();

	/*
	 * When there is a power source change, or if the battery
	 * state of charge (soc) is less than or equal to BATTERY_LEVEL_LOW,
	 * inform the AP.
	 */
	if ((current_port != prev_charge_port) ||
		(prev_power_source != current_power_source) ||
		(on_battery && batt_soc <= BATTERY_LEVEL_LOW)) {

		int ac_power_src;
		int artg;

		/* AC removal or low battery */
		if (current_power_source == POWER_SOURCE_BATTERY) {
			/* DC source */
			ac_power_src = DC_SOURCE;

			/* Adapter rating is zero for DC source */
			artg = 0;

			/* Increment sequence number and take care not to overflow */
			pd_state_sequence = (pd_state_sequence + 1) % PD_STATE_SEQUENCE_MAX;

			/* Throttle action */
			throttle_ap(THROTTLE_ON, THROTTLE_HARD,
					THROTTLE_SRC_BAT_DISCHG_CURRENT);

			/* Deassert PROCHOT, if not done within 2seconds */
			hook_call_deferred(&deassert_prochot_data, 2 * SECOND);

			prev_charge_port = CHARGE_PORT_NONE;
		} else {
			/* AC Source is USBC */
			ac_power_src = AC_SOURCE_USBC;

			prev_charge_port = current_port;

			/* calculate adapter rating */
			artg = get_adapter_rating();

			/* In EC memory, artg is bits[11:0].
			 * So, artg is saved in 50mW units.
			 * AP must convert it back to mW units,
			 * when sending to DPTF.
			 */
			artg = artg / 50;
		}

		*memmap_psrc = (ac_power_src | (pd_state_sequence << 4));
		*memmap_artg = artg;

		/* Send SCI Event */
		pd_send_host_event(PD_EVENT_POWER_CHANGE);
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
	uint8_t *memmap_psrc =  host_get_memmap(EC_MEMMAP_PWR_SRC);
	uint16_t *memmap_artg = (uint16_t *)host_get_memmap(EC_MEMMAP_PWR_ARTG);

	/* Initial Value */
	*memmap_psrc = 0;
	*memmap_artg = 0;

	/* Update the initial information on power source */
	update_power_source();
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);

/*
 * Function to retrieve the pd sequence
 */
static int get_cached_pd_sequence(void)
{
	int cached_pd_sequence;
	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);

	cached_pd_sequence = (((*memmap_psrc) & PD_SEQUENCE_MASK) >> 4);

	return cached_pd_sequence;
}

/*
 * Function to handle power boss policy
 */
void dptf_handle_pbok(int pbok_sequence)
{
	/*
	 * Deassert PROCHOT, only if sequence number received
	 * from AP matches to the cached value at EC.
	 */
	if (pbok_sequence == get_cached_pd_sequence()) {

		hook_call_deferred(&deassert_prochot_data, 0);

		CPRINTS("Power Boss Policy OK! PROCHOT Deasserted!");
	}
}
