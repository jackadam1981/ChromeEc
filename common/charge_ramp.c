/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Charge input current limit ramp module for Chrome EC */

#include "charge_manager.h"
#include "charge_ramp.h"
#include "common.h"
#include "console.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_USBCHARGE, format, ## args)

/* Maximum charging current to ramp to */
#define MAX_CHARGING_CURRENT_MA 3000

/* Number of times to ramp current searching for limit before stable charging */
#define RAMP_COUNT          3

/* Time to delay for detecting the charger type */
#define CHARGE_DETECT_DELAY (2*SECOND)

/* Maximum allowable time charger can be unplugged to be considered an OCP */
#define OC_RECOVER_MAX_TIME SECOND

/* Current ramp increment */
#define RAMP_CURR_INCR_MA   64
#define RAMP_CURR_DELAY     SECOND

/* How much to backoff the input current limit when limit has been found */
#define RAMP_ICL_BACKOFF    (2*RAMP_CURR_INCR_MA)

/* Interval at which VBUS voltage is monitored in stable state */
#define RAMP_VBUS_MONITOR_INTERVAL (5*SECOND)

enum chg_ramp_state {
	CHG_RAMP_DISCONNECTED,
	CHG_RAMP_CHARGE_DETECT,
	CHG_RAMP_OVERCURRENT_DETECT,
	CHG_RAMP_RAMP,
	CHG_RAMP_STABLE
};
static enum chg_ramp_state ramp_st;

struct oc_info {
	timestamp_t ts;
	uint64_t recover;
	int port;
	int sup;
	int icl;
};
/* OCP info for each over-current */
static struct oc_info oc_info[RAMP_COUNT];
static int oc_info_idx;

/* Active charging information */
static int active_port = CHARGE_PORT_NONE;
static int active_sup;
static int active_icl;

/* Minimum input current limit for active charger */
static int min_icl;


void chg_ramp_charge_supplier_change(int port, int supplier)
{
	/*
	 * If the last active port was a valid port and the supplier
	 * is absent, then this may have been an over-current.
	 */
	if (active_port != CHARGE_PORT_NONE &&
	    port == CHARGE_PORT_NONE) {
		oc_info_idx = (oc_info_idx == RAMP_COUNT - 1) ?
				0 : oc_info_idx + 1;
		oc_info[oc_info_idx].ts = get_time();
		oc_info[oc_info_idx].port = active_port;
		oc_info[oc_info_idx].sup = active_sup;
		oc_info[oc_info_idx].icl = active_icl;
	}

	/* Set new active port, set ramp state, and wake ramp task */
	active_port = port;
	active_sup = supplier;
	min_icl = 0;
	ramp_st = (active_port == CHARGE_PORT_NONE) ? CHG_RAMP_DISCONNECTED :
						      CHG_RAMP_CHARGE_DETECT;
	task_wake(TASK_ID_CHG_RAMP);
}

void chg_ramp_set_min_current(int current)
{
	min_icl = current;
}

void chg_ramp_task(void)
{
	int task_wait_time = -1;
	int i;
	/*
	 * Static initializer so that we don't clobber early calls to this
	 * module.
	 */
	static enum chg_ramp_state ramp_st_prev = CHG_RAMP_DISCONNECTED,
				   ramp_st_new = CHG_RAMP_DISCONNECTED;
	int active_icl_new;

	/* Clear last OCP port to guarantee we ramp on first connect */
	oc_info[0].port = CHARGE_PORT_NONE;

	while (1) {
		ramp_st_new = ramp_st;
		active_icl_new = active_icl;
		switch (ramp_st) {
		case CHG_RAMP_DISCONNECTED:
			/* Do nothing */
			task_wait_time = -1;
			break;
		case CHG_RAMP_CHARGE_DETECT:
			/* On entry to state, store the OC recovery time */
			if (ramp_st_prev != ramp_st)
				oc_info[oc_info_idx].recover =
					get_time().val -
					oc_info[oc_info_idx].ts.val;

			/*
			 * If we are not drawing full charge, then don't ramp,
			 * just wait in this state, until we are.
			 */
			if (!board_is_consuming_full_charge()) {
				task_wait_time = 5 * SECOND;
				break;
			}

			/* Delay for charge_manager to determine supplier */
			ramp_st_new = CHG_RAMP_OVERCURRENT_DETECT;
			task_wait_time = CHARGE_DETECT_DELAY;
			break;
		case CHG_RAMP_OVERCURRENT_DETECT:
			task_wait_time = SECOND;

			/* Skip ramp for specific suppliers */
			if (!board_is_ramp_allowed(active_sup)) {
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_STABLE;
				break;
			}

			/*
			 * Compare recent OCP events, if all info matches,
			 * then we don't need to ramp anymore.
			 */
			for (i = 0; i < RAMP_COUNT; i++) {
				if (oc_info[i].port != active_port ||
				    oc_info[i].sup != active_sup ||
				    oc_info[i].recover > OC_RECOVER_MAX_TIME)
					/* TODO: compare ICL also */
					break;
			}

			if (i == RAMP_COUNT) {
				/* Found OC threshold! */
				active_icl_new = oc_info[oc_info_idx].icl -
						 RAMP_ICL_BACKOFF;
				ramp_st_new = CHG_RAMP_STABLE;
			} else {
				/*
				 * Need to ramp to find OC threshold, start
				 * at the minimum input current limit.
				 */
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_RAMP;
			}
			break;
		case CHG_RAMP_RAMP:
			task_wait_time = RAMP_CURR_DELAY;

			/* Pause ramping if we are not drawing full current */
			if (!board_is_consuming_full_charge()) {
				task_wait_time = 5 * SECOND;
				break;
			}

			/* If VBUS is sagging a lot, then stop ramping */
			if (board_is_vbus_too_low()) {
				CPRINTS("VBUS too low");
				active_icl_new = active_icl - RAMP_ICL_BACKOFF;
				ramp_st_new = CHG_RAMP_STABLE;
				break;
			}

			/* Ramp the current limit if we haven't reached max */
			if (active_icl == MAX_CHARGING_CURRENT_MA)
				ramp_st_new = CHG_RAMP_STABLE;
			else if (active_icl + RAMP_CURR_INCR_MA >
				 MAX_CHARGING_CURRENT_MA)
				active_icl_new = MAX_CHARGING_CURRENT_MA;
			else
				active_icl_new = active_icl + RAMP_CURR_INCR_MA;

			break;
		case CHG_RAMP_STABLE:
			/* Keep an eye on VBUS and restart ramping if it dips */
			if (board_is_vbus_too_low()) {
				CPRINTS("VBUS low; Re-ramp");
				active_icl_new = min_icl;
				ramp_st_new = CHG_RAMP_RAMP;
			}
			task_wait_time = RAMP_VBUS_MONITOR_INTERVAL;
			break;
		}
		if (ramp_st_prev != ramp_st || active_icl != active_icl_new)
			CPRINTS("CHG RAMP: %d->%d, %dmA, %dmA",
				ramp_st_prev, ramp_st, min_icl, active_icl_new);

		ramp_st_prev = ramp_st;
		ramp_st = ramp_st_new;
		active_icl = active_icl_new;

		/*
		 * If we are ramping or stable, then use the active input
		 * current limit. Otherwise, use the minimum input current
		 * limit.
		 */
		if (ramp_st == CHG_RAMP_RAMP || ramp_st == CHG_RAMP_STABLE)
			board_set_charge_limit(active_icl);
		else
			board_set_charge_limit(min_icl);

		task_wait_event(task_wait_time);
	}
}

#ifdef CONFIG_CMD_CHGRAMP
static int command_chgramp(int argc, char **argv)
{
	int i;

	ccprintf("Chg Ramp:\nState: %d\nMin ICL: %d\nActive ICL: %d\n",
		 ramp_st, min_icl, active_icl);

	ccprintf("OC idx:%d\n", oc_info_idx);
	for (i = 0; i < RAMP_COUNT; i++) {
		ccprintf("OC %d: p%d s%d recover%lu icl%d\n", i,
			 oc_info[i].port, oc_info[i].sup,
			 oc_info[i].recover, oc_info[i].icl);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(chgramp, command_chgramp,
	"",
	"Dump charge ramp state info",
	NULL);
#endif
