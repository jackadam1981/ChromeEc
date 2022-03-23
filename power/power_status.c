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

/* pd_state_sequence is 8 bit number */
#define PD_STATE_SEQUENCE_MAX 15

#define PROCHOT_DEASSERT_NOT_OK 0
#define PROCHOT_DEASSERT_OK	1

/* PROCHOT action based on PD state change sequence number */
static int prochot_action = PROCHOT_DEASSERT_OK;

/* Current system power source */
static enum system_power_source current_power_source = POWER_SOURCE_UNKNOWN;

/* Deasset PROCHOT, if already asserted.
 * PROCHOT requires to be deasserted either based on Power boss OK ACK
 * or within 2seconds of no recieval of Power boss OK ACK.
 */
static void deassert_prochot(void)
{
	if (prochot_action == PROCHOT_DEASSERT_NOT_OK) {

		prochot_action = PROCHOT_DEASSERT_OK;

		/* deassert PROCHOT */
		throttle_ap(THROTTLE_OFF, THROTTLE_HARD,
			THROTTLE_SRC_BAT_DISCHG_CURRENT);
	}
}
DECLARE_DEFERRED(deassert_prochot);

/* Function evaluates the current source of power and then returns true,
 * if the power source is either on battery or on both battery & AC.
 */
bool get_latest_power_source(void)
{
	const bool batt_pres = battery_is_present() == BP_YES;
	const int ext_pres = extpower_is_present();

	/* Determine new power source */
	if (ext_pres && batt_pres) {
		current_power_source = POWER_SOURCE_AC_BATTERY;
		CPRINTS("PTOM -AC+ BATT");
	} else if (ext_pres) {
		current_power_source = POWER_SOURCE_AC;
	} else if (batt_pres) {
		current_power_source = POWER_SOURCE_BATTERY;
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
	bool on_battery;

	/* PD state change sequence number */
	static int pd_state_sequence;

	uint8_t *memmap_psrc =  host_get_memmap(EC_MEMMAP_PWR_SRC);

	/* Get the percentage of state of charge */
	if (IS_ENABLED(CONFIG_USB_POWER_DELIVERY))
		batt_soc = usb_get_battery_soc();

	on_battery = get_latest_power_source();

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

			/* Increment sequence number and
			 * take care not to overflow.
			 */
			pd_state_sequence = (pd_state_sequence ==
					PD_STATE_SEQUENCE_MAX) ?
					0 : pd_state_sequence + 1;

			/* Throttle action */
			prochot_action = PROCHOT_DEASSERT_NOT_OK;
			throttle_ap(THROTTLE_ON, THROTTLE_HARD,
					THROTTLE_SRC_BAT_DISCHG_CURRENT);
		} else {
			/* AC Source is USBC */
			ac_power_src = AC_SOURCE_USBC;
		}
		#endif /* CONFIG_CHARGE_MANAGER */

		*memmap_psrc = (ac_power_src | (pd_state_sequence << 4));

		/* Send SCI Event */
		pd_send_host_event(PD_EVENT_POWER_CHANGE);

		/* Deassert PROCHOT, if not done within 2seconds */
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
	uint8_t *memmap_psrc =  host_get_memmap(EC_MEMMAP_PWR_SRC);
	int *memmap_pbok = (int *)host_get_memmap(EC_MEMMAP_PWR_PBOK);

	/* Initial Value */
	*memmap_psrc = 0;
	/* Default value of bit:31 - deassert_ok */
	*memmap_pbok = (PROCHOT_DEASSERT_OK << 31);

	/* Get an initial information on power source */
	update_power_source();
}
DECLARE_HOOK(HOOK_INIT, power_status_init, HOOK_PRIO_LAST);

/*
 * Retrieve the memory content of PSRC.
 * Bits [3:0] indicates the power source and the bits [7:4] indicates
 * the changed power delivery state sequence number.
 */
int dptf_get_psrc(void)
{
	int result;
	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);

	result = *memmap_psrc;
	CPRINTS("PSRC : 0x%0x", result);
	return result;
}

void dptf_handle_pbok(int pbok_sequence)
{
	int *memmap_pbok = (int *)host_get_memmap(EC_MEMMAP_PWR_PBOK);
	uint8_t *memmap_psrc = host_get_memmap(EC_MEMMAP_PWR_SRC);

	int cached_pd_sequence = ((*memmap_psrc & 0xF0) >> 4);

	/*
	 * Deassert PROCHOT, only if sequence number received from AP
	 * matches to the cahed value at EC as well as if PROCHOT is
	 * already asserted.
	 */
	if ((prochot_action == PROCHOT_DEASSERT_OK) &&
			(pbok_sequence == cached_pd_sequence)) {

		hook_call_deferred(&deassert_prochot_data, 0);

		*memmap_pbok = (pbok_sequence |
				(prochot_action << 31));
		CPRINTS("Power Boss Policy OK! PROCHOT Deasserted!");
	}
}
