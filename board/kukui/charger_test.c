/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "battery.h"
#include "charge_manager.h"
#include "console.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

static const int IDLE_DURATION = 1 * HOUR;

static enum {
	CHARGING,
	DISCHARGING,
	IDLE
} state;

timestamp_t idle_deadline;

void charger_test_task(void *u)
{
	usleep(10 * SECOND); /* wait for other components initialized */

	state = CHARGING;
	while (1) {
		struct batt_params batt;
		
		battery_get_params(&batt);
		CPRINTS("loop: state=%d, battery=%d", (int)state,
				batt.state_of_charge);

		switch (state) {
		case CHARGING:
			if (batt.state_of_charge >= 99) {
				state = IDLE;
				idle_deadline.val = get_time().val + IDLE_DURATION;
			}
			break;
		case DISCHARGING:
			if (batt.state_of_charge < 90) {
				charge_manager_set_override(OVERRIDE_OFF);
				state = CHARGING;
			}
			break;
		case IDLE:
			if (timestamp_expired(idle_deadline, NULL)) {
				charge_manager_set_override(OVERRIDE_DONT_CHARGE);
				state = DISCHARGING;
			}
			break;
		}

		usleep(10 * SECOND);

	}
}
