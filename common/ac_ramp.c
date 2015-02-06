/* Copyright (c) 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* AC input current limit ramp module for Chrome EC */

#include "ac_ramp.h"
#include "charge_manager.h"
#include "common.h"
#include "console.h"
#include "task.h"
#include "timer.h"
#include "util.h"

/* Number of times to ramp current searching for limit before stable charging */
#define RAMP_COUNT          3

/* Time to delay for detecting the charger type */
#define CHARGE_DETECT_DELAY (2*SECOND)

/* Maximum allowable time charger can be unplugged to be considered an OCP */
#define OC_RECOVER_MAX_TIME SECOND

/* Current ramp increment */
#define RAMP_CURR_INCR_MA   64
#define RAMP_CURR_DELAY     SECOND

enum ac_ramp_state {
	AC_RAMP_DISCONNECTED,
	AC_RAMP_CHARGE_DETECT,
	AC_RAMP_OVERCURRENT_DETECT,
	AC_RAMP_RAMP,
	AC_RAMP_STABLE
};

struct oc_info {
	timestamp_t ts;
	int oc_recover;
	int port;
	int sup;
	int icl;
};

static enum ac_ramp_state st;

static struct oc_info oc_info[RAMP_COUNT];
static int oc_info_idx;

static int active_port = CHARGE_PORT_NONE;
static int active_sup;
static int active_icl;
static int min_icl;


void ac_ramp_charge_port_change(int new_port)
{
	/*
	 * If the last active port was a valid port, then this may
	 * have been an over-current.
	 */
	if (active_port != CHARGE_PORT_NONE) {
		oc_info_idx = (oc_info_idx == RAMP_COUNT - 1) ?
						0 : oc_info_idx + 1;
		oc_info[oc_info_idx].ts = get_time();
		oc_info[oc_info_idx].port = active_port;
		oc_info[oc_info_idx].sup = active_sup;
		oc_info[oc_info_idx].icl = active_icl;
	}

	/* Set new active port, set ramp state, and wake ramp task */
	active_port = new_port;
	st = (active_port == CHARGE_PORT_NONE) ? AC_RAMP_DISCONNECTED :
						 AC_RAMP_CHARGE_DETECT;
	task_wake(TASK_ID_AC_RAMP);
}

void ac_ramp_set_baseline_current(int current)
{
	min_icl = current;
}

void ac_ramp_task(void)
{
	int task_wait_time = -1;
	int i;
	enum ac_ramp_state st_prev = AC_RAMP_DISCONNECTED, st_new;

	/* Clear last OCP port to guarantee we ramp on first connect */
	oc_info[0].port = CHARGE_PORT_NONE;

	while (1) {
		task_wait_event(task_wait_time);

		ccprintf("AC RAMP: %d->%d", st_prev, st);

		st_new = st;
		switch (st) {
		case AC_RAMP_DISCONNECTED:
			/* Do nothing */
			task_wait_time = -1;
			break;
		case AC_RAMP_CHARGE_DETECT:
			/* On entry to state, store the OC recovery time */
        		if (st_prev != st)
				oc_info[oc_info_idx].oc_recover =
					get_time().val -
					oc_info[oc_info_idx].ts.val;

			/*
			 * If we are not drawing full charge, then don't ramp,
			 * just wait in this state, until we are.
			 */
			if (!board_is_full_charging()) {
				task_wait_time = 5 * SECOND;
				break;
			}

			/* Delay for charge_manager to determine supplier */
			st_new = AC_RAMP_OVERCURRENT_DETECT;
			task_wait_time = CHARGE_DETECT_DELAY;
			break;
		case AC_RAMP_OVERCURRENT_DETECT:
			/* Get active supplier */
			active_sup = charge_manager_get_active_supplier();

			/*
			 * Compare recent OCP events, if all infor matches,
			 * then we don't need to ramp anymore.
			 */
			for (i = 0; i < RAMP_COUNT; i++) {
				if (oc_info[i].port != active_port ||
				    oc_info[i].sup != active_sup ||
				    oc_info[i].oc_recover > OC_RECOVER_MAX_TIME)
					/* TODO: compare ICL also */
					break;
			}

			/* TODO: check if we should ramp at all with this supplier */
			if (i == RAMP_COUNT) {
				/* Found OC threshold! */
				active_icl = oc_info[oc_info_idx].icl -
						2 * RAMP_CURR_INCR_MA;
				st_new = AC_RAMP_STABLE;
			} else {
				/*
				 * Need to ramp to find OC threshold, start
				 * at the minimum input current limit.
				 */
				active_icl = min_icl;
				st_new = AC_RAMP_RAMP;
			}

			task_wait_time = SECOND;
			break;
		case AC_RAMP_RAMP:
			/* If VBUS is sagging a lot, then stop ramping */
			if (board_is_vbus_too_low()) {
				st_new = AC_RAMP_STABLE;
				task_wait_time = SECOND;
				break;
			}

			/* Ramp the current limit */
			active_icl += RAMP_CURR_INCR_MA;
			task_wait_time = RAMP_CURR_DELAY;
			break;
		case AC_RAMP_STABLE:
			/* Do nothing */
			task_wait_time = -1;
			break;
		}
		st_prev = st;
		st = st_new;

		ccprintf(", %dmA, %dmA\n", min_icl, active_icl);

		/*
		 * If we are ramping or stable, then use the active input
		 * current limit. Otherwise, use the minimum input current
		 * limit.
		 */
		if (st == AC_RAMP_RAMP || st == AC_RAMP_STABLE)
			board_set_charge_limit(active_icl);
		else
			board_set_charge_limit(min_icl);
	}
}


static int command_acramp(int argc, char **argv)
{
	int i;

	ccprintf("AC Ramp:\nState: %d\nMin ICL: %d\nActive ICL: %d\n",
		 st, min_icl, active_icl);

	ccprintf("OC idx:%d\n", oc_info_idx);
	for (i = 0; i < RAMP_COUNT; i++) {
		ccprintf("OC %d: p%d s%d recover%d icl%d\n", i, oc_info[i].port,
			 oc_info[i].sup, oc_info[i].oc_recover, oc_info[i].icl);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(acramp, command_acramp,
	"",
	"Dump AC Ramp state info",
	NULL);
