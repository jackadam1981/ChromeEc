/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery pack vendor provided charging profile
 */

#include "battery.h"
#include "battery_fuel_gauge.h"
#include "battery_smart.h"
#include "gpio.h"
#include "hooks.h"
#include "system.h"

static enum battery_present batt_pres_prev = BP_NOT_SURE;

enum battery_present __attribute__((weak)) variant_battery_present(void)
{
	return BP_NOT_SURE;
}

enum battery_present battery_hw_present(void)
{
	enum battery_present bp = variant_battery_present();

	if (bp != BP_NOT_SURE)
		return bp;

	return gpio_get_level(GPIO_EC_BATT_PRES_ODL) ? BP_NO : BP_YES;
}

static int battery_init(void)
{
	int batt_status;

	return battery_status(&batt_status) ? 0 :
		!!(batt_status & STATUS_INITIALIZED);
}

enum battery_disconnect_grace_period {
	BATTERY_DISCONNECT_GRACE_PERIOD_OFF,
	BATTERY_DISCONNECT_GRACE_PERIOD_ON,
	BATTERY_DISCONNECT_GRACE_PERIOD_OVER,
};
static enum battery_disconnect_grace_period disconnect_grace_period;

static void battery_disconnect_timer(void)
{
	disconnect_grace_period = BATTERY_DISCONNECT_GRACE_PERIOD_OVER;
}
DECLARE_DEFERRED(battery_disconnect_timer);

__overridable
enum battery_disconnect_state variant_battery_check_disconnect(void)
{
	return BATTERY_NOT_DISCONNECTED;
}

static enum battery_disconnect_state battery_check_disconnect(void)
{
	if (!battery_init())
		return BATTERY_DISCONNECT_ERROR;

	return variant_battery_check_disconnect();
}

/*
 * Check for case where both XCHG and XDSG bits are set indicating that even
 * though the FG can be read from the battery, the battery is not able to be
 * charged or discharged. This situation will happen if a battery disconnect was
 * initiated via H1 setting the DISCONN signal to the battery. This will put the
 * battery pack into a sleep state and when power is reconnected, the FG can be
 * read, but the battery is still not able to provide power to the system. The
 * calling function returns batt_pres = BP_NO, which instructs the charging
 * state machine to prevent powering up the AP on battery alone which could lead
 * to a brownout event when the battery isn't able yet to provide power to the
 * system. .
 */
enum battery_disconnect_state battery_check_disconnect_ti_bq40z50(void)
{
	int rv;
	uint8_t data[6];

	/* Check if battery charging + discharging is disabled. */
	rv = sb_read_mfgacc(PARAM_OPERATION_STATUS,
			    SB_ALT_MANUFACTURER_ACCESS, data, sizeof(data));
	if (rv)
		return BATTERY_DISCONNECT_ERROR;

	if ((data[3] & (BATTERY_DISCHARGING_DISABLED |
			BATTERY_CHARGING_DISABLED)) ==
	    (BATTERY_DISCHARGING_DISABLED | BATTERY_CHARGING_DISABLED)) {
		/*
		 * We need a workaround to wake up a battery from cutoff. We
		 * return DISCONNECT_ERROR for the 5 seconds after the first
		 * call BP_NOT_SURE is reported to chgstv2. It will supply
		 * precharge current and wakes up the battery from cutoff. If
		 * the battery is good, we won't come back here.  If not, after
		 * 5 seconds, we will return DISCONNECTED to stop charging and
		 * avoid damaging the battery.
		 */
		if (disconnect_grace_period ==
				BATTERY_DISCONNECT_GRACE_PERIOD_OVER)
			return BATTERY_DISCONNECTED;
		if (disconnect_grace_period ==
				BATTERY_DISCONNECT_GRACE_PERIOD_OFF)
			hook_call_deferred(&battery_disconnect_timer_data,
					   5 * SECOND);
		ccprintf("Battery disconnect grace period\n");
		disconnect_grace_period = BATTERY_DISCONNECT_GRACE_PERIOD_ON;
		return BATTERY_DISCONNECT_ERROR;
	}

	return BATTERY_NOT_DISCONNECTED;
}

/*
 * Physical detection of battery.
 */
static enum battery_present battery_check_present_status(void)
{
	enum battery_present batt_pres;
	enum battery_disconnect_state batt_disconnect_status;

	/* Get the physical hardware status */
	batt_pres = battery_hw_present();

	/*
	 * If the battery is not physically connected, then no need to perform
	 * any more checks.
	 */
	if (batt_pres != BP_YES)
		return batt_pres;

	/*
	 * If the battery is present now and was present last time we checked,
	 * return early.
	 */
	if (batt_pres == batt_pres_prev)
		return batt_pres;

	batt_disconnect_status = battery_check_disconnect();
	if (batt_disconnect_status == BATTERY_DISCONNECT_ERROR)
		return BP_NOT_SURE;

	/*
	 * Ensure that battery is:
	 * 1. Not in cutoff
	 * 2. Initialized
	 */
	if (battery_is_cut_off() != BATTERY_CUTOFF_STATE_NORMAL ||
	    battery_init() == 0) {
		batt_pres = BP_NO;
	}

	return batt_pres;
}

enum battery_present battery_is_present(void)
{
	batt_pres_prev = battery_check_present_status();
	return batt_pres_prev;
}
