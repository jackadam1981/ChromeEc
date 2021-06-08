/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Primus specific PWM LED settings. */

#include "battery.h"
#include "charge_manager.h"
#include "charge_state.h"
#include "chipset.h"
#include "common.h"
#include "ec_commands.h"
#include "extpower.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "timer.h"
#include "util.h"
#define CPRINTS(format, args...) cprints(CC_LED, format, ## args)

#define LED_ON_LVL		100
#define LED_OFF_LVL		0
#define LED_BAT_S3_OFF_TIME_MS	3000
#define LED_BAT_S3_TICK_MS	50
#define LED_BAT_S3_PWM_RESCALE	5
#define LED_TOTAL_TICKS		6
#define TICKS_STEP1_BRIGHTER	0
#define TICKS_STEP2_DIMMER	(1000 / LED_BAT_S3_TICK_MS)
#define TICKS_STEP3_OFF		(2 * TICKS_STEP2_DIMMER)
#define LED_ONE_SEC		(1000 / HOOK_TICK_INTERVAL_MS)
#define LED_LOGO_TICK_SEC	(0.25 * LED_ONE_SEC)
/* Total on/off duration in a period */
#define PERIOD			(LED_LOGO_TICK_SEC * 2)
#define LED_OFF			EC_LED_COLOR_COUNT

static int tick;

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED,
	EC_LED_ID_POWER_LED
};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

void led_set_color_battery(enum ec_led_colors color)
{
	switch (color) {
	case EC_LED_COLOR_AMBER:
		pwm_set_duty(PWM_CH_LED1, LED_ON_LVL);
		pwm_set_duty(PWM_CH_LED2, LED_OFF_LVL);
		break;
	case EC_LED_COLOR_WHITE:
		pwm_set_duty(PWM_CH_LED2, LED_ON_LVL);
		pwm_set_duty(PWM_CH_LED1, LED_OFF_LVL);
		break;
	default: /* LED_OFF and other unsupported colors */
		pwm_set_duty(PWM_CH_LED1, LED_OFF_LVL);
		pwm_set_duty(PWM_CH_LED2, LED_OFF_LVL);
		break;
	}
}

static void led_set_battery(void)
{
	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* Always indicate when charging, even in suspend. */
		led_set_color_battery(EC_LED_COLOR_AMBER);
		break;
	case PWR_STATE_DISCHARGE:
		led_set_color_battery(LED_OFF);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		led_set_color_battery(EC_LED_COLOR_WHITE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

void led_set_color_power(enum ec_led_colors color)
{
	if (color == EC_LED_COLOR_RED)
		pwm_set_duty(PWM_CH_TKP_A_LED_N, LED_ON_LVL);
	else
		/* LED_OFF and unsupported colors */
		pwm_set_duty(PWM_CH_TKP_A_LED_N, LED_OFF_LVL);
}

static void led_set_power(void)
{
	static int plug_ac;
	static int ticks;
	/* Count how many times we enter this loop when plug in AC */
	static int counts;
	static int color;
	/* Record on or off phase */
	int phase;

	if (plug_ac) {

		if (counts > LED_TOTAL_TICKS) {
			/* Clear this flag once on/off repeat 3 times */
			plug_ac = 0;
			return;
		}
		phase = ticks < (int)LED_LOGO_TICK_SEC ?
								0 : 1;
		ticks = (ticks + 1) % (int)PERIOD;
		color = phase == 1 ?
				EC_LED_COLOR_RED : LED_OFF;
		led_set_color_power(color);

		counts++;

	} else if (chipset_in_state(CHIPSET_STATE_ON))
		led_set_color_power(EC_LED_COLOR_RED);
	else if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		led_set_color_power(LED_OFF);

	/* Set plug_ac for initial connection of power
	 * Check AC_PRESENT
	 * enter led_set_power first time
	 * counts is smaller than 6, which means we didn't flick LED 3 times.
	 */
	if (extpower_is_present() && (!plug_ac || counts <= LED_TOTAL_TICKS)) {
		plug_ac = 1;
	} else if (!extpower_is_present()) {
		plug_ac = 0;
		counts = 0;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		brightness_range[EC_LED_COLOR_AMBER] = 1;
		brightness_range[EC_LED_COLOR_WHITE] = 1;
	} else if (led_id == EC_LED_ID_POWER_LED) {
		brightness_range[EC_LED_COLOR_RED] = 1;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	if (led_id == EC_LED_ID_BATTERY_LED) {
		if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(EC_LED_COLOR_AMBER);
		else if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(EC_LED_COLOR_WHITE);
		else
			led_set_color_battery(LED_OFF);
	} else if (led_id == EC_LED_ID_POWER_LED) {
		if (brightness[EC_LED_COLOR_RED] != 0)
			led_set_color_power(EC_LED_COLOR_RED);
		else
			led_set_color_power(LED_OFF);
	}

	return EC_SUCCESS;
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

static void suspend_led_update_deferred(void);
DECLARE_DEFERRED(suspend_led_update_deferred);

static void suspend_led_update_deferred(void)
{
	/* every 50ms enter this loop again. */
	int delay = LED_BAT_S3_TICK_MS * MSEC;

	tick++;

	/* 1s gradual on, 1s gradual off, 3s off */
	if (tick <= TICKS_STEP2_DIMMER) {
		/* increase 5 duty every 50ms until PWM=100 */
		/* enter here 20 times, total duartion is 1sec */
		pwm_set_duty(PWM_CH_TKP_A_LED_N,
			tick * LED_BAT_S3_PWM_RESCALE);
	} else if (tick <= TICKS_STEP3_OFF) {
		/* decrease 5 duty every 50ms until PWM=0 */
		/* enter here 20 times, total duartion is 1sec */
		pwm_set_duty(PWM_CH_TKP_A_LED_N,
			(TICKS_STEP3_OFF - tick) * LED_BAT_S3_PWM_RESCALE);
	} else {
		tick = TICKS_STEP1_BRIGHTER;
		delay = LED_BAT_S3_OFF_TIME_MS * MSEC;
	}

	hook_call_deferred(&suspend_led_update_deferred_data, delay);
}

static void suspend_led_init(void)
{
	/* Keep led on when enter suspend to avoid a short off */
	tick = TICKS_STEP2_DIMMER;

	hook_call_deferred(&suspend_led_update_deferred_data, 0);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, suspend_led_init, HOOK_PRIO_DEFAULT);

static void suspend_led_deinit(void)
{
	hook_call_deferred(&suspend_led_update_deferred_data, -1);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, suspend_led_deinit, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, suspend_led_deinit, HOOK_PRIO_DEFAULT);
