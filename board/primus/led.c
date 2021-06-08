/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Primus specific PWM LED settings. */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "cros_board_info.h"
#include "common.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "led_onoff_states.h"
#include "led_pwm.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"

#define LED_ON_LVL 0
#define LED_OFF_LVL 1
#define LED_BAT_OFF_LVL	0
#define LED_BAT_ON_LVL	1
#define LED_BAT_S3_OFF_TIME_MS 3000
#define LED_BAT_S3_PWM_RESCALE 5
#define LED_BAT_S3_TICK_MS 50

#define LED_TOTAL_TICKS 2
#define LED_ON_TICKS 1

#define LED_PWR_TICKS_PER_CYCLE 7

#define TICKS_STEP1_BRIGHTER 0
#define TICKS_STEP2_DIMMER 20
#define TICKS_STEP3_OFF 40

static int ticks;

__override const int led_charge_lvl_1 = 5;

__override const int led_charge_lvl_2 = 95;

__override struct led_descriptor
      led_bat_state_table[LED_NUM_STATES][LED_NUM_PHASES] = {
  [STATE_CHARGING_LVL_1]       = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
  [STATE_CHARGING_LVL_2]       = {{EC_LED_COLOR_AMBER, LED_INDEFINITE} },
  [STATE_CHARGING_FULL_CHARGE] = {{EC_LED_COLOR_WHITE, LED_INDEFINITE} },
  [STATE_DISCHARGE_S0]       = {{LED_OFF,  LED_INDEFINITE} },
  [STATE_DISCHARGE_S3]       = {{EC_LED_COLOR_RED,  LED_INDEFINITE},
                                {LED_OFF,       3 * LED_ONE_SEC} },
  [STATE_DISCHARGE_S5]         = {{EC_LED_COLOR_RED,  LED_INDEFINITE} },
};

__override const struct led_descriptor
    led_pwr_state_table[PWR_LED_NUM_STATES][LED_NUM_PHASES] = {
  [PWR_LED_STATE_ON]           =  {{EC_LED_COLOR_RED, LED_INDEFINITE} },
  [PWR_LED_STATE_SUSPEND_AC]   =  {{EC_LED_COLOR_WHITE,  1 * LED_ONE_SEC},
    {LED_OFF,             0.25 * LED_ONE_SEC} },
  [PWR_LED_STATE_OFF]           = {
    {LED_OFF,             LED_INDEFINITE} },
};

/* TODO(b/190637023)
 * Need to implement specific LED feature for Primus.
 */
const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_LEFT_LED,
	EC_LED_ID_RIGHT_LED,
};

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

/*
 * We only have a white and an amber LED, so setting any other colour results in
 * both LEDs being off.
 */
struct pwm_led led_color_map[EC_LED_COLOR_COUNT] = {
				/* Amber, White */
	[EC_LED_COLOR_RED]    = {   0,   0 },
	[EC_LED_COLOR_GREEN]  = {   0,   0 },
	[EC_LED_COLOR_BLUE]   = {   0,   0 },
	[EC_LED_COLOR_YELLOW] = {   0,   0 },
	[EC_LED_COLOR_WHITE]  = {   0, 100 },
	[EC_LED_COLOR_AMBER]  = {  100,  0 },
};

/* Two logical LEDs with amber and white channels. */
struct pwm_led pwm_leds[CONFIG_LED_PWM_COUNT] = {
	{
		.ch0 = PWM_CH_LED1,
		.ch1 = PWM_CH_LED2,
		.ch2 = PWM_LED_NO_CHANNEL,
		.enable = &pwm_enable,
		.set_duty = &pwm_set_duty,
	},
	{
		.ch0 = PWM_CH_TKP_A_LED_N,
		.ch1 = PWM_CH_LED4,
		.ch2 = PWM_LED_NO_CHANNEL,
		.enable = &pwm_enable,
		.set_duty = &pwm_set_duty,
	},
};

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	memset(brightness_range, '\0',
	       sizeof(*brightness_range) * EC_LED_COLOR_COUNT);
	brightness_range[EC_LED_COLOR_AMBER] = 100;
	brightness_range[EC_LED_COLOR_WHITE] = 100;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	enum pwm_led_id pwm_id;

	/* Convert ec_led_id to pwm_led_id. */
	switch (led_id) {
	case EC_LED_ID_LEFT_LED:
		pwm_id = PWM_LED0;
		break;
	case EC_LED_ID_RIGHT_LED:
		pwm_id = PWM_LED1;
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}

	if (brightness[EC_LED_COLOR_WHITE])
		set_pwm_led_color(pwm_id, EC_LED_COLOR_WHITE);
	else if (brightness[EC_LED_COLOR_AMBER])
		set_pwm_led_color(pwm_id, EC_LED_COLOR_AMBER);
	else
		/* Otherwise, the "color" is "off". */
		set_pwm_led_color(pwm_id, -1);

	return EC_SUCCESS;
}

static void suspend_led_update_deferred(void);
DECLARE_DEFERRED(suspend_led_update_deferred);

static void suspend_led_update_deferred(void)
{
	int delay = LED_BAT_S3_TICK_MS * MSEC;

	ticks++;

	/* 1s gradual on, 1s gradual off, 3s off */
	if (ticks <= TICKS_STEP2_DIMMER) {
		pwm_set_duty(PWM_CH_LED4, ticks * LED_BAT_S3_PWM_RESCALE);
	} else if (ticks <= TICKS_STEP3_OFF) {
		pwm_set_duty(PWM_CH_LED4,
					(TICKS_STEP3_OFF - ticks) * LED_BAT_S3_PWM_RESCALE);
	} else {
		ticks = TICKS_STEP1_BRIGHTER;
		delay = LED_BAT_S3_OFF_TIME_MS * MSEC;
	}

	hook_call_deferred(&suspend_led_update_deferred_data, delay);
}

/* PWM brightness vs. color, in the order of off, white */
//static const uint8_t color_brightness[2] = {
	//[LED_OFF]   = 0,
	//[LED_WHITE]   = 100,
//};


enum led_color {
	//LED_OFF = 0,
	LED_WHITE,
	LED_AMBER,
	LED_RED,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

void led_set_color_power(enum ec_led_colors color)
{
	//pwm_set_duty(PWM_CH_LED4, color_brightness[color]);
}

void led_set_color_battery(enum ec_led_colors color)
{
	/*uint32_t board_ver = 0;
	int led_batt_on_lvl, led_batt_off_lvl;

	cbi_get_board_version(&board_ver);
	if (board_ver >= 3) {
		led_batt_on_lvl = LED_BAT_ON_LVL;
		led_batt_off_lvl = LED_BAT_OFF_LVL;
	} else {
		led_batt_on_lvl = !LED_BAT_ON_LVL;
		led_batt_off_lvl = !LED_BAT_OFF_LVL;
	}*/

	switch (color) {
	case LED_AMBER:
		//gpio_set_level(GPIO_LED_FULL_L, led_batt_off_lvl);
		//gpio_set_level(GPIO_LED_CHRG_L, led_batt_on_lvl);
		break;
	case LED_WHITE:
		//gpio_set_level(GPIO_LED_FULL_L, led_batt_on_lvl);
		//gpio_set_level(GPIO_LED_CHRG_L, led_batt_off_lvl);
		break;
	default: /* LED_OFF and other unsupported colors */
		//gpio_set_level(GPIO_LED_FULL_L, led_batt_off_lvl);
		//gpio_set_level(GPIO_LED_CHRG_L, led_batt_off_lvl);
		break;
	}
}

static void led_set_battery(void)
{
	static int battery_ticks;
	uint32_t chflags = charge_get_flags();

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		led_set_color_battery(LED_AMBER);
		break;
	case PWR_STATE_DISCHARGE:
		led_set_color_battery(LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		led_set_color_battery(LED_WHITE);
		break;
	case PWR_STATE_IDLE: /* External power connected in IDLE */
		if (chflags & CHARGE_FLAG_FORCE_IDLE)
			led_set_color_battery(
				(battery_ticks & 0x4) ? LED_AMBER : LED_OFF);
		else
			led_set_color_battery(LED_WHITE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void led_set_power(void)
{
	static int power_ticks;
	static int previous_state_suspend;
	static int blink_ticks;

	power_ticks++;

	/* Blink 3 times (0.25s on/0.25s off, repeat 3 times) */
	if (extpower_is_present()) {
		blink_ticks++;
		if (!previous_state_suspend)
			power_ticks = 0;

		while (blink_ticks < LED_PWR_TICKS_PER_CYCLE) {
			led_set_color_power(
				(power_ticks % LED_TOTAL_TICKS) < LED_ON_TICKS ?
				LED_RED : LED_OFF);

			previous_state_suspend = 1;
			return;
		}
	}
	if (!extpower_is_present())
		blink_ticks = 0;

	previous_state_suspend = 0;

	if (chipset_in_state(CHIPSET_STATE_SOFT_OFF))
		led_set_color_power(LED_OFF);
	if (chipset_in_state(CHIPSET_STATE_ON))
		led_set_color_power(LED_RED);
}


/* Called by hook task every 200 ms */
static void led_tick(void)
{
	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED))
		led_set_power();
	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
		led_set_battery();
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);