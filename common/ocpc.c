/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* OCPC - One Charger IC Per Type-C module */

#include "battery.h"
#include "charge_state_v2.h"
#include "charger.h"
#include "common.h"
#include "console.h"
#include "math_util.h"
#include "ocpc.h"
#include "util.h"

/*
 * These constants were chosen by tuning the PID loop to reduce oscillations and
 * minimize overshoot.
 */
#define KP 1
#define KP_DIV 4
#define KI 1
#define KI_DIV 15
#define KD 1
#define KD_DIV 10

/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHARGER, outstr)
#define CPRINTS(format, args...) cprints(CC_CHARGER, format, ## args)
#define CPRINTS_DBG(format, args...) \
do {							\
	if (debug_output)				\
		cprints(CC_CHARGER, format, ## args);	\
} while (0)

static int k_p = KP;
static int k_i = KI;
static int k_d = KD;
static int debug_output;

int ocpc_config_secondary_charger(void *curr, struct ocpc_data *ocpc,
				  int voltage_mv, int current_ma)
{
	int rv = EC_SUCCESS;
	struct batt_params batt;
	const struct battery_info *batt_info;
	struct charger_params charger;
	int vsys_target = 0;
	int drive = 0;
	int i_ma;
	int desired_input_current;
	int min_vsys_target;
	int error = 0;
	int derivative = 0;

	/* There's nothing to do if we're not using this charger. */
	if (charge_get_active_chg_chip() != SECONDARY_CHARGER)
		return EC_ERROR_INVAL;

	if (current_ma == 0) {
		vsys_target = voltage_mv;
		goto set_vsys;
	}

	/*
	 * We need to induce a current flow that matches the requested current
	 * by raising VSYS.  Let's start by getting the latest data that we
	 * know of.
	 */
	batt_info = battery_get_info();
	battery_get_params(&batt);
	ocpc_get_adcs(ocpc);
	charger_get_params(&charger);

	/* Set our current target accordingly. */
	if (batt.voltage < batt.desired_voltage)
		i_ma = batt.desired_current;
	else
		i_ma = MAX(batt.current, 0);

	/* Ensure our target is not negative. */
	i_ma = MAX(i_ma, 0);

	/*
	 * We'll use our current target and our combined Rsys+Rbatt to seed our
	 * VSYS target.  However, we'll use a PID loop to correct the error and
	 * help drive VSYS to what it _should_ be in order to reach our current
	 * target.  The first time through this function, we won't make any
	 * corrections in order to determine our initial error.
	 */
	if (ocpc->last_vsys != OCPC_UNINIT) {
		error = i_ma - batt.current;
		/* Add some hysteresis. */
		if (ABS(error) < 4)
			error = 0;

		derivative = error - ocpc->last_error;
		ocpc->last_error = error;
		ocpc->integral +=  k_i * error;
		if (ocpc->integral > 500)
			ocpc->integral = 500;
	}

	CPRINTS_DBG("error = %dmA", error);
	CPRINTS_DBG("derivative = %d", derivative);
	CPRINTS_DBG("integral = %d", ocpc->integral);
	CPRINTS_DBG("batt.voltage = %dmV", batt.voltage);
	CPRINTS_DBG("batt.desired_voltage = %dmV", batt.desired_voltage);
	CPRINTS_DBG("batt.desired_current = %dmA", batt.desired_current);
	CPRINTS_DBG("batt.current = %dmA", batt.current);
	CPRINTS_DBG("i_ma = %dmA", i_ma);

	/*
	 * Assuming that our combined Rsys + Rbatt resistance is correct, this
	 * should be enough to reach our desired i_ma.  If it's not, our PID
	 * loop will help us get there.
	 */
	min_vsys_target = (i_ma * ocpc->combined_rsys_rbatt_mo) / 1000;
	min_vsys_target += MIN(batt.voltage, batt.desired_voltage);
	CPRINTS_DBG("min_vsys_target = %d", min_vsys_target);

	/* Obtain the drive from our PID controller. */
	if (ocpc->last_vsys != OCPC_UNINIT) {
		drive = (k_p * error / KP_DIV) + (ocpc->integral / KI_DIV) +
			(k_d * derivative / KD_DIV);
		/*
		 * Let's limit upward transitions to 500mV.  It's okay to reduce
		 * VSYS rather quickly, but we'll be conservative on
		 * increasing VSYS.
		 */
		if (drive > 500)
			drive = 500;
		CPRINTS_DBG("drive = %d", drive);
	}

	/*
	 * Adjust our VSYS target by applying the calculated drive.  Note that
	 * we won't apply our drive the first time through this function such
	 * that we can determine our initial error.
	 */
	if (ocpc->last_vsys != OCPC_UNINIT)
		vsys_target = ocpc->last_vsys + drive;

	/*
	 * Ensure VSYS is no higher than 1V over the max battery voltage, but
	 * greater than or equal to our minimum VSYS target.
	 */
	vsys_target = CLAMP(vsys_target, min_vsys_target,
			    batt_info->voltage_max+1000);

	/* If we're input current limited, we cannot increase VSYS any more. */
	desired_input_current = ((struct charge_state_data
				  *)curr)->desired_input_current;
	CPRINTS_DBG("OCPC: Inst. Input Current: %dmA (Limit: %dmA)",
		    ocpc->secondary_ibus_ma, desired_input_current);
	if ((ocpc->secondary_ibus_ma >= (desired_input_current * 95 / 100)) &&
	    (vsys_target > ocpc->last_vsys)) {
		CPRINTS("Input limited! Not increasing VSYS");
		return rv;
	}

set_vsys:
	/* To reduce spam, only print when we change VSYS significantly. */
	if ((ABS(vsys_target - ocpc->last_vsys) > 10) || debug_output)
		CPRINTS("OCPC: Target VSYS: %dmV", vsys_target);
	charger_set_current(SECONDARY_CHARGER, current_ma);
	charger_set_voltage(SECONDARY_CHARGER, vsys_target);
	ocpc->last_vsys = vsys_target;

	return rv;
}

void ocpc_get_adcs(struct ocpc_data *ocpc)
{
	int val;

	val = 0;
	if (!charger_get_vbus_voltage(PRIMARY_CHARGER, &val))
		ocpc->primary_vbus_mv = val;

	val = 0;
	if (!charger_get_vbus_voltage(SECONDARY_CHARGER, &val))
		ocpc->secondary_vbus_mv = val;

	val = 0;
	if (!charger_get_input_current(PRIMARY_CHARGER, &val))
		ocpc->primary_ibus_ma = val;

	val = 0;
	if (!charger_get_input_current(SECONDARY_CHARGER, &val))
		ocpc->secondary_ibus_ma = val;
}

static int command_ocpcdebug(int argc, char **argv)
{
	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	if (!parse_bool(argv[1], &debug_output))
		return EC_ERROR_PARAM1;

	return EC_SUCCESS;
}
DECLARE_SAFE_CONSOLE_COMMAND(ocpcdebug, command_ocpcdebug,
			     "<enable/disable>",
			     "enable/disable debug prints for OCPC data");
