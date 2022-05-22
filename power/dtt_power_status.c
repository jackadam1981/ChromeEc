/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Support Power Participant in Dynamic Tuning Technology */

#include "battery.h"
#include "battery_smart.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "dptf.h"
#include "dtt_power_status.h"
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

/* pd_state_sequence is 4 bit number */
#define PD_STATE_SEQUENCE_MAX	0x0F

/* pd_sequence corresponds to bit[0:3] in PSRC */
#define PD_SEQUENCE_MASK	0xF0

/* Rerieve cached pd sequence */
#define CACHED_PD_SEQUENCE(mem)	((*(mem) & PD_SEQUENCE_MASK) >> 4)

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

static int dbpt_initialized = false;

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
 * or within 2seconds of no recieval of Power boss OK ACK.
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

	int batt_soc;
	int ac_power_src;
	int artg;
	bool on_battery;

	/* PD state change sequence number */
	static int pd_state_sequence;

	uint8_t *memmap_psrc =  host_get_memmap(EC_MEMMAP_PWR_SRC);
	uint16_t *memmap_artg = (uint16_t *)host_get_memmap(EC_MEMMAP_PWR_ARTG);

	/* Get the percentage of state of charge */
	if (IS_ENABLED(CONFIG_USB_POWER_DELIVERY))
		batt_soc = usb_get_battery_soc();

	on_battery = is_latest_power_source_with_battery();

	/*
	 * When there is a power source change, or if the battery
	 * state of charge (soc) is less than or equal to BATTERY_LEVEL_LOW,
	 * inform the AP.
	 */
	if ((prev_power_source != current_power_source) ||
		(on_battery && batt_soc <= BATTERY_LEVEL_LOW)) {

#ifdef CONFIG_CHARGE_MANAGER
		int chg_port;
		/* Check if the charger port has been disconnected
		 * or low battery scenario.
		 */
		chg_port = charge_manager_get_active_charge_port();

		if ((pd_is_disconnected(chg_port)) ||
			(current_power_source == POWER_SOURCE_BATTERY)) {

			/* Not an AC source, but DC source */
			ac_power_src = DC_SOURCE;

			/* Adapter rating is zero for DC source */
			artg = 0;

			/* Increment sequence number and
			 * take care not to overflow.
			 */
			pd_state_sequence = (pd_state_sequence ==
					PD_STATE_SEQUENCE_MAX) ?
					0 : pd_state_sequence + 1;

			/* Throttle action */
			throttle_ap(THROTTLE_ON, THROTTLE_HARD,
					THROTTLE_SRC_BAT_DISCHG_CURRENT);

			/* Deassert PROCHOT, if not done within 2seconds */
			hook_call_deferred(&deassert_prochot_data, 2 * SECOND);

		} else {
			/* AC Source is USBC */
			ac_power_src = AC_SOURCE_USBC;

			/* calculate adapter rating */
			artg = get_adapter_rating();

			/* In EC memory, artg is bits[11:0].
			 * So, artg is saved in 50mW units.
			 * AP must convert it back to mW units,
			 * when sending to DPTF.
			 */
			artg = artg / 50;
		}
#endif /* CONFIG_CHARGE_MANAGER */

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

#ifdef CONFIG_BATTERY_DBPT_V2PLUS
void update_dbpt(void)
{
	static int count;
	uint16_t batt_max_peak_power, batt_sus_peak_power;
	uint16_t batt_high_freq_impedance;
	uint16_t prev_batt_max_peak_power, prev_batt_sus_peak_power;
	uint16_t prev_batt_high_freq_impedance;
	uint16_t threshold_max_power_change, threshold_sus_peak_power_change;
	uint16_t threshold_high_freq_impedance_change;

	/* return if fuel guage is not initialized */
	if (!dbpt_initialized)
		return;

	/* Read from fuel guage */
	batt_max_peak_power = battery_maximum_power();
	batt_sus_peak_power = battery_sustained_power();
	batt_high_freq_impedance = battery_high_frequency_impedance();

	prev_batt_max_peak_power =
			*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PMAX));
	prev_batt_sus_peak_power =
			*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PBSS));
	prev_batt_high_freq_impedance =
			*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_RBHF));

	/* Check for any threshold level change */
	if (prev_batt_max_peak_power < batt_max_peak_power) {
			threshold_max_power_change = (batt_max_peak_power -
							prev_batt_max_peak_power);
	} else {
			threshold_max_power_change = (prev_batt_max_peak_power -
							batt_max_peak_power);
	}

	if (prev_batt_sus_peak_power < batt_sus_peak_power) {
			threshold_sus_peak_power_change = (batt_sus_peak_power -
							prev_batt_sus_peak_power);
	} else {
			threshold_sus_peak_power_change = (prev_batt_sus_peak_power -
							batt_sus_peak_power);
	}

	if (prev_batt_high_freq_impedance < batt_high_freq_impedance) {
			threshold_high_freq_impedance_change =
					(batt_high_freq_impedance -
					 prev_batt_high_freq_impedance);
	} else {
			threshold_high_freq_impedance_change =
					(prev_batt_high_freq_impedance -
					 batt_high_freq_impedance);
	}

	/* update ec_memmap regions */
	*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PMAX)) =
                                                        batt_max_peak_power;
	*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PBSS)) =
							batt_sus_peak_power;
	*((uint16_t *)host_get_memmap(EC_MEMMAP_BATT_RBHF)) =
						batt_high_freq_impedance;

	/* Send sci event for any threshold level cross */
	if ((threshold_max_power_change >= 250) ||
			       (threshold_sus_peak_power_change >= 100) ||
			       (threshold_high_freq_impedance_change >= 5)) {

		host_set_single_event(EC_HOST_EVENT_BATTERY_STATUS);
	}

}
DECLARE_HOOK(HOOK_SECOND, update_dbpt, HOOK_PRIO_DEFAULT);

static void init_dbpt(void)
{
	int rv;
	rv = battery_set_sys_resistance(batt_param.sys_resistance);
	if (rv) {
		dbpt_initialized = false;
		return;
	}

	rv = battery_set_min_sys_voltage(batt_param.min_sys_volt);
	if (rv) {
		dbpt_initialized = false;
		return;
	}
	CPRINTS("DBPT initialised!");
	dbpt_initialized = true;

	/*
	 * Update the mememap with initial value, after initial configuration.
	 */
	update_dbpt();
}

#endif /* CONFIG_BATTERY_DBPT_V2PLUS */

static void power_status_init(void)
{
	uint8_t *memmap_psrc =  host_get_memmap(EC_MEMMAP_PWR_SRC);
	uint16_t *memmap_artg = (uint16_t *)host_get_memmap(EC_MEMMAP_PWR_ARTG);
	uint16_t *memmap_pmax = (uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PMAX);
	uint16_t *memmap_pbss = (uint16_t *)host_get_memmap(EC_MEMMAP_BATT_PBSS);
	uint16_t *memmap_rbhf = (uint16_t *)host_get_memmap(EC_MEMMAP_BATT_RBHF);

	/* Initial Value */
	*memmap_psrc = 0;
	*memmap_artg = 0;
	*memmap_pmax = 0;
	*memmap_pbss = 0;
	*memmap_rbhf = 0;

	/* Update the initial information on power source */
	update_power_source();
#ifdef CONFIG_BATTERY_DBPT_V2PLUS
	/* Initialize for enabling DBPT capability */
	init_dbpt();
#endif
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);

/*
 * Function to handle power boss policy
 */
void dptf_handle_pbok(int pbok_sequence)
{
	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);


	/*
	 * Deassert PROCHOT, only if sequence number received
	 * from AP matches to the cached value at EC.
	 */
	if (pbok_sequence == CACHED_PD_SEQUENCE(memmap_psrc)) {

		hook_call_deferred(&deassert_prochot_data, 0);

		CPRINTS("Power Boss Policy OK! PROCHOT Deasserted!");
	}
}
