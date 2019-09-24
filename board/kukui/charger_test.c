/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "console.h"
#include "led_common.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

static const int IDLE_DURATION = 30 * MINUTE;

static enum TestState {
	CHARGING,
	DISCHARGING,
	IDLE
} state;

timestamp_t idle_deadline;

void update_led(enum TestState state, int cycle)
{
	uint8_t br[EC_LED_COLOR_COUNT] = {};
	uint8_t zero[EC_LED_COLOR_COUNT] = {};

	switch (state) {
	case CHARGING:
		br[EC_LED_COLOR_BLUE] = 1;
	case DISCHARGING:
		br[EC_LED_COLOR_RED] = 1;
	case IDLE:
		br[EC_LED_COLOR_GREEN] = 1;
	}

	for (int i = __fls(cycle); i >= 0; i--) {
		led_set_brightness(EC_LED_ID_BATTERY_LED, br);
		if (cycle & (1 << i))
			usleep(1500 * MSEC);
		else
			usleep(500 * MSEC);
		led_set_brightness(EC_LED_ID_BATTERY_LED, zero);
		usleep(500 * MSEC);
	}
}

void charger_test_task(void *u)
{
	int cycle = 1;

	usleep(10 * SECOND); /* wait for other components initialized */

	state = CHARGING;
	while (1) {
		const struct batt_params *batt = charger_current_battery_params();

		CPRINTS("loop %d: state=%d, battery=%d", cycle, (int)state,
				batt->state_of_charge);

		switch (state) {
		case CHARGING:
			if (batt->state_of_charge >= 97) {
				state = IDLE;
				idle_deadline.val = get_time().val + IDLE_DURATION;
			}
			break;
		case DISCHARGING:
			if (batt->state_of_charge < 90) {
				charge_manager_set_override(OVERRIDE_OFF);
				state = CHARGING;
				++cycle;
			} else {
				/* poke ap to make discharge faster */
				chipset_reset(CHIPSET_RESET_CONSOLE_CMD);
			}
			break;
		case IDLE:
			if (timestamp_expired(idle_deadline, NULL)) {
				charge_manager_set_override(OVERRIDE_DONT_CHARGE);
				state = DISCHARGING;
			}
			break;
		}

		update_led(state, cycle);

		usleep(5 * SECOND);
	}
}
