/* Copyright 2022 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Crota specific PWM LED settings. */

#include <stdint.h>

#include "charge_state.h"
#include "compile_time_macros.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"

#define LED_INDEFINITE UINT8_MAX
#define LED_ONE_SEC (1000 / HOOK_TICK_INTERVAL_MS)
#define LED_OFF EC_LED_COLOR_COUNT
#define BAT_LED_ON_LVL 100
#define BAT_LED_OFF_LVL 0

const int led_charge_lvl = 96;

struct led_descriptor {
	enum ec_led_colors color;
	uint8_t time;
};

enum led_phase { LED_PHASE_0, LED_PHASE_1, LED_NUM_PHASES };

enum led_states {
	STATE_CHARGING,
	STATE_CHARGING_FULL_CHARGE,
	STATE_DISCHARGE,
	STATE_DISCHARGE_BAT_LOW,
	STATE_BATTERY_ERROR,
	STATE_FACTORY_TEST,
	LED_NUM_STATES
};

static const struct led_descriptor
	led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
		[STATE_CHARGING] = { { EC_LED_COLOR_WHITE,
					     LED_INDEFINITE } },
		[STATE_CHARGING_FULL_CHARGE] = { { LED_OFF, LED_INDEFINITE } },
		[STATE_DISCHARGE] = { { LED_OFF, LED_INDEFINITE } },
		[STATE_DISCHARGE_BAT_LOW] = { { EC_LED_COLOR_AMBER,
						   LED_INDEFINITE } },
		[STATE_BATTERY_ERROR] = { { EC_LED_COLOR_AMBER,
					    1 * LED_ONE_SEC },
					  { LED_OFF, 1 * LED_ONE_SEC } },
		[STATE_FACTORY_TEST] = { { EC_LED_COLOR_WHITE,
					   1 * LED_ONE_SEC },
					 { LED_OFF, 1 * LED_ONE_SEC } },
	};

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

static int led_get_charge_percent(void)
{
	return DIV_ROUND_NEAREST(charge_get_display_charge(), 10);
}

void led_set_color_battery(enum ec_led_colors color)
{
	/* There are two battery leds, LED1/LED2 are on MB side.
	 * All leds are OFF by default.
	 */
	int led1_duty, led2_duty;

	led1_duty = led2_duty = BAT_LED_OFF_LVL;

	switch (color) {
	case EC_LED_COLOR_AMBER:
		led1_duty = BAT_LED_ON_LVL;
		break;
	case EC_LED_COLOR_WHITE:
		led2_duty = BAT_LED_ON_LVL;
		break;
	default: /* LED_OFF and other unsupported colors */
		break;
	}

	pwm_set_duty(PWM_CH_LED1, led1_duty);
	pwm_set_duty(PWM_CH_LED2, led2_duty);
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_AMBER] = 1;
	brightness_range[EC_LED_COLOR_WHITE] = 1;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	led_auto_control(led_id, 0);
	if (brightness[EC_LED_COLOR_AMBER] != 0)
		led_set_color_battery(EC_LED_COLOR_AMBER);
	else if (brightness[EC_LED_COLOR_WHITE] != 0)
		led_set_color_battery(EC_LED_COLOR_WHITE);
	else
		led_set_color_battery(LED_OFF);

	return EC_SUCCESS;
}

/* Custom led on off states control */
static enum led_states led_get_state(void)
{
	int charge_lvl;
	enum led_states new_state = LED_NUM_STATES;

	if (!IS_ENABLED(CONFIG_CHARGER))
		return new_state;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Get percent charge */
		charge_lvl = led_get_charge_percent();
		/* Determine which charge state to use */
		if (charge_lvl < led_charge_lvl)
			new_state = STATE_CHARGING;
		else
			new_state = STATE_CHARGING_FULL_CHARGE;
		break;
	case PWR_STATE_DISCHARGE_FULL:
		if (extpower_is_present()) {
			new_state = STATE_CHARGING_FULL_CHARGE;
			break;
		}
		__fallthrough;
	case PWR_STATE_DISCHARGE /* and PWR_STATE_DISCHARGE_FULL */:
		if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			new_state = STATE_DISCHARGE;
		else{
#ifdef CONFIG_LED_ONOFF_STATES_BAT_LOW
			if (led_get_charge_percent() <
			    CONFIG_LED_ONOFF_STATES_BAT_LOW)
				new_state = STATE_DISCHARGE_BAT_LOW;
			else
#endif
				new_state = STATE_DISCHARGE;
		}
		break;
	case PWR_STATE_ERROR:
		new_state = STATE_BATTERY_ERROR;
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		new_state = STATE_CHARGING_FULL_CHARGE;
		break;
	/* Make sure when battery is pre-charging, the LED will blinking.
	 * Otherwise it will wait 30 seconds then blinking.
	 */
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		new_state = STATE_BATTERY_ERROR;
		break;
	case PWR_STATE_FORCED_IDLE:
		new_state = STATE_FACTORY_TEST;
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}

	return new_state;
}

static void led_update_battery(void)
{
	static uint8_t ticks, period;
	static int led_state = LED_NUM_STATES;
	int phase;
	enum led_states desired_state = led_get_state();

	/*
	 * We always need to check the current state since the value could
	 * have been manually overwritten. If we're in a new valid state,
	 * update our ticks and period info. If our new state isn't defined,
	 * continue using the previous one.
	 */
	if (desired_state != led_state && desired_state < LED_NUM_STATES) {
		/* State is changing */
		led_state = desired_state;
		/* Reset ticks and period when state changes */
		ticks = 0;

		period = led_bat_state_table[led_state][LED_PHASE_0].time +
			 led_bat_state_table[led_state][LED_PHASE_1].time;
	}

	/* If this state is undefined, turn the LED off */
	if (period == 0) {
		led_set_color_battery(LED_OFF);
		return;
	}

	/*
	 * Determine which phase of the state table to use. The phase is
	 * determined if it falls within first phase time duration.
	 */
	phase = ticks < led_bat_state_table[led_state][LED_PHASE_0].time ? 0 :
									   1;
	ticks = (ticks + 1) % period;

	/* Set the color for the given state and phase */
	led_set_color_battery(led_bat_state_table[led_state][phase].color);
}

static void led_init(void)
{
	/* If battery LED is enabled, set it to "off" to start with */
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		led_set_color_battery(LED_OFF);
}
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_DEFAULT);

/* Called by hook task every hook tick (200 msec) */
static void led_update(void)
{
	/*
	 * If battery LED is enabled, set its state based on our power and
	 * charge
	 */
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		led_update_battery();
}
DECLARE_HOOK(HOOK_TICK, led_update, HOOK_PRIO_DEFAULT);
