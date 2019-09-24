/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "crc8.h"
#include "led_common.h"
#include "link_defs.h"
#include "power.h"
#include "system.h"
#include "task.h"
#include "timer.h"
#include "util.h"

#define CPRINTS(format, args...) cprints(CC_SPI, format, ## args)
#define CPRINTF(format, args...) cprintf(CC_SPI, format, ## args)

static const int IDLE_DURATION = 30 * MINUTE;

enum TestState {
	CHARGING,
	DISCHARGING,
	IDLE
};

timestamp_t idle_deadline;

#define PRESERVED_VAR(type, name) static type name \
	__uncached __preserved_logs(name)

PRESERVED_VAR(int, charger_test_cycle);
PRESERVED_VAR(enum TestState, charger_test_state);
PRESERVED_VAR(uint8_t, charger_test_checksum);

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

uint8_t charger_test_calc_checksum(void)
{
	uint8_t checksum = 0;

	checksum = crc8((uint8_t*)&charger_test_cycle, sizeof(charger_test_cycle));
	checksum = crc8_arg((uint8_t*)&charger_test_state, sizeof(charger_test_state),
			    checksum);
	checksum = crc8_arg((uint8_t*)__TIME__, 8, checksum);

	return checksum;
}

void charger_test_update_state(int cycle, enum TestState state)
{
	charger_test_cycle = cycle;
	charger_test_state = state;
	charger_test_checksum = charger_test_calc_checksum();
}

void charger_test_init(void)
{
	if (charger_test_checksum != charger_test_calc_checksum()) {
		charger_test_update_state(1, CHARGING);
	}
}

void charger_test_task(void *u)
{
	usleep(10 * SECOND); /* wait for other components initialized */

	charger_test_init();

	while (1) {
		const struct batt_params *batt = charger_current_battery_params();

		CPRINTS("loop %d: state=%d, battery=%d",
				charger_test_cycle,
				(int)charger_test_state,
				batt->state_of_charge);

		switch (charger_test_state) {
		case CHARGING:
			charge_manager_set_override(OVERRIDE_OFF);
			if (batt->state_of_charge >= 97) {
				charger_test_update_state(charger_test_cycle, IDLE);
				idle_deadline.val = get_time().val + IDLE_DURATION;
			} else if (power_get_state() == POWER_S0) {
				chipset_force_shutdown(CHIPSET_SHUTDOWN_CONSOLE_CMD);
			}
			break;
		case DISCHARGING:
			charge_manager_set_override(OVERRIDE_DONT_CHARGE);
			if (batt->state_of_charge < 90) {
				charger_test_update_state(charger_test_cycle + 1, CHARGING);
			} else if (power_get_state() != POWER_S0) {
				chipset_reset(CHIPSET_RESET_CONSOLE_CMD);
			}
			break;
		case IDLE:
			if (timestamp_expired(idle_deadline, NULL)) {
				charger_test_update_state(charger_test_cycle, DISCHARGING);
			} else if (power_get_state() == POWER_S0) {
				chipset_force_shutdown(CHIPSET_SHUTDOWN_CONSOLE_CMD);
			}
			break;
		}

		update_led(charger_test_state, charger_test_cycle);

		usleep(5 * SECOND);
	}
}
