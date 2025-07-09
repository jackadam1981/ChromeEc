/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "battery.h"
#include "board_led.h"
#include "charge_state.h"
#include "chipset.h"
#include "cros_cbi.h"
#include "ec_commands.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "led_common.h"
#include "led_pwm.h"
#include "timer.h"
#include "util.h"

#include <stdint.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(brox_led, LOG_LEVEL_INF);

#define BATT_LOW_BCT 8

#define LED_TICKS_PER_CYCLE 4
#define LED_ON_TICKS 2

#define LED_PWM_PERIOD_NS BOARD_LED_HZ_TO_PERIOD_NS(324)

/*
 * Due to the CSME-Lite processing, upon startup the CPU transitions through
 * S0->S3->S5->S3->S0, causing the LED to turn on/off/on, so
 * delay turning off power LED during suspend/shutdown.
 */
#define PWR_LED_CPU_DELAY K_MSEC(2000)

const enum ec_led_id supported_led_ids[] = { EC_LED_ID_BATTERY_LED,
					     EC_LED_ID_POWER_LED };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_WHITE,
	LED_COLOR_COUNT /* Number of colors, not a color itself */
};

static const struct board_led_pwm_dt_channel pwr_led_white =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(DT_NODELABEL(pwm_power_led_white));
static const struct board_led_pwm_dt_channel bat_led_white =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(
		DT_NODELABEL(pwm_battery_led_white));
static const struct board_led_pwm_dt_channel bat_led_amber =
	BOARD_LED_PWM_DT_CHANNEL_INITIALIZER(
		DT_NODELABEL(pwm_battery_led_amber));

static void pwm_led_set_duty(const struct board_led_pwm_dt_channel *ch,
			     int percent)
{
	uint32_t pulse_ns;
	int rv;

	if (!device_is_ready(ch->dev)) {
		LOG_ERR("device %s not ready", ch->dev->name);
		return;
	}

	pulse_ns = DIV_ROUND_NEAREST(LED_PWM_PERIOD_NS * percent, 100);

	LOG_DBG("PWM LED %s set percent (%d), pulse %d", ch->dev->name, percent,
		pulse_ns);

	rv = pwm_set(ch->dev, ch->channel, LED_PWM_PERIOD_NS, pulse_ns,
		     ch->flags);
	if (rv) {
		LOG_ERR("pwm_set() failed %s (%d)", ch->dev->name, rv);
	}
}

static void led_set_color_battery(enum led_color color, int duty)
{
	/* PWM LED duty range from 0% ~ 100% */
	if (duty < 0 || 100 < duty) {
		LOG_ERR("Try to set PWM battery LED duty %d%%"
			"which is not from 0%% ~ 100%%",
			duty);
		return;
	}

	switch (color) {
	case LED_WHITE:
		pwm_led_set_duty(&bat_led_white, duty);
		pwm_led_set_duty(&bat_led_amber, 0);
		break;
	case LED_AMBER:
		pwm_led_set_duty(&bat_led_white, 0);
		pwm_led_set_duty(&bat_led_amber, duty);
		break;
	case LED_OFF:
		pwm_led_set_duty(&bat_led_white, 0);
		pwm_led_set_duty(&bat_led_amber, 0);
		break;
	default:
		break;
	}
}

static void led_set_color_power(enum led_color color, int duty)
{
	/* PWM LED duty range from 0% ~ 100% */
	if (duty < 0 || 100 < duty) {
		LOG_ERR("Try to set PWM battery LED duty %d%%"
			"which is not from 0%% ~ 100%%",
			duty);
		return;
	}

	switch (color) {
	case LED_WHITE:
		pwm_led_set_duty(&pwr_led_white, duty);
		break;
	case LED_OFF:
		pwm_led_set_duty(&pwr_led_white, 0);
		break;
	default:
		break;
	}
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 100;
		brightness_range[EC_LED_COLOR_AMBER] = 100;
		break;
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 100;
		break;
	default:
		break;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_battery(LED_WHITE,
					      brightness[EC_LED_COLOR_WHITE]);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_battery(LED_AMBER,
					      brightness[EC_LED_COLOR_AMBER]);
		else
			led_set_color_battery(LED_OFF, 0);
		break;
	case EC_LED_ID_POWER_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_power(LED_WHITE,
					    brightness[EC_LED_COLOR_WHITE]);
		else
			led_set_color_power(LED_OFF, 0);
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}

#define BATT_LOW_LED_PULSE_MS (875 * USEC_PER_MSEC)
#define BATT_CRI_LED_PULSE_MS (375 * USEC_PER_MSEC)
#define BATT_LED_ON_TIME_MS (125 * USEC_PER_MSEC)
#define BATT_LOW_LED_PULSE_TICK_MS (35 * USEC_PER_MSEC)
#define BATT_CRI_LED_PULSE_TICK_MS (75 * USEC_PER_MSEC)

static void batt_pwm_led_tick(void);
DECLARE_DEFERRED(batt_pwm_led_tick);

static struct {
	uint32_t interval;
	int duty_inc;
	enum led_color color;
	uint32_t on_time;
	int duty;
} batt_led_pulse;

#define BATT_PWM_LED_CONFIG_TICK(interval, interval_period, color)             \
	batt_led_config_tick((interval), 100 / (interval_period / (interval)), \
			     (color), (BATT_LED_ON_TIME_MS))

static void batt_led_config_tick(uint32_t interval, int duty_inc,
				 enum led_color color, uint32_t on_time)
{
	batt_led_pulse.interval = interval;
	batt_led_pulse.duty_inc = duty_inc;
	batt_led_pulse.color = color;
	batt_led_pulse.on_time = on_time;
	batt_led_pulse.duty = 0;
}

static void batt_pwm_led_tick(void)
{
	uint32_t elapsed;
	uint32_t next = 0;
	uint32_t start = get_time().le.lo;

	if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
		if (batt_led_pulse.duty == 0) {
			batt_led_pulse.duty = 100;
			next = batt_led_pulse.on_time;
		} else if ((batt_led_pulse.duty - batt_led_pulse.duty_inc) < 0)
			batt_led_pulse.duty = 0;
		else
			batt_led_pulse.duty -= batt_led_pulse.duty_inc;

		led_set_color_battery(batt_led_pulse.color,
				      batt_led_pulse.duty);
	}

	if (next == 0)
		next = batt_led_pulse.interval;
	elapsed = get_time().le.lo - start;
	next = next > elapsed ? next - elapsed : 0;
	hook_call_deferred(&batt_pwm_led_tick_data, next);
}

static void led_set_battery(void)
{
	static unsigned int battery_ticks;
	static bool batt_cri_triggered, batt_low_triggered;

	battery_ticks++;

	switch (led_pwr_get_state()) {
	case LED_PWRS_CHARGE:
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
			led_set_color_battery(LED_AMBER, 100);
		break;
	case LED_PWRS_DISCHARGE:
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
			if (charge_get_percent() <= BATTERY_LEVEL_CRITICAL &&
			    !batt_cri_triggered) {
				/*
				 * Flashing amber LED quickly if battery is
				 * under critical level
				 */
				batt_cri_triggered = 1;
				batt_low_triggered = 0;
				BATT_PWM_LED_CONFIG_TICK(
					BATT_CRI_LED_PULSE_TICK_MS,
					BATT_CRI_LED_PULSE_MS, LED_AMBER);
				hook_call_deferred(&batt_pwm_led_tick_data, 0);
			} else if (charge_get_percent() < BATT_LOW_BCT &&
				   charge_get_percent() >
					   BATTERY_LEVEL_CRITICAL &&
				   !batt_low_triggered) {
				/*
				 * Flashing amber LED slowly if battery is lower
				 * than 8% while not reaching critical level
				 */
				batt_low_triggered = 1;
				batt_cri_triggered = 0;
				BATT_PWM_LED_CONFIG_TICK(
					BATT_LOW_LED_PULSE_TICK_MS,
					BATT_LOW_LED_PULSE_MS, LED_AMBER);
				hook_call_deferred(&batt_pwm_led_tick_data, 0);
			} else if (charge_get_percent() >= BATT_LOW_BCT) {
				/*
				 * Turn off LED, cancel flashing task and
				 * restore triggered record if battery is above
				 * 8%
				 */
				batt_cri_triggered = 0;
				batt_low_triggered = 0;
				hook_call_deferred(&batt_pwm_led_tick_data, -1);
				led_set_color_battery(LED_OFF, 0);
			}
		} else {
			batt_cri_triggered = 0;
			batt_low_triggered = 0;
			hook_call_deferred(&batt_pwm_led_tick_data, -1);
		}
		break;
	case LED_PWRS_ERROR:
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
			/* Blinking amber LED quickly if battery error */
			led_set_color_battery(
				(battery_ticks & 0x1) ? LED_AMBER : LED_OFF,
				100);
		break;
	case LED_PWRS_CHARGE_NEAR_FULL:
	case LED_PWRS_IDLE: /* External power connected in IDLE */
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
			led_set_color_battery(LED_WHITE, 100);
		break;
	case LED_PWRS_FORCED_IDLE:
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED))
			led_set_color_battery(
				(battery_ticks % LED_TICKS_PER_CYCLE <
				 LED_ON_TICKS) ?
					LED_AMBER :
					LED_OFF,
				100);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void led_set_power(void)
{
	static int power_ticks;

	power_ticks++;

	if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) {
		if (chipset_in_state(CHIPSET_STATE_ON))
			led_set_color_power(LED_WHITE, 100);
		else if (chipset_in_state(CHIPSET_STATE_ANY_SUSPEND))
			led_set_color_power((power_ticks % LED_TICKS_PER_CYCLE <
					     LED_ON_TICKS) ?
						    LED_WHITE :
						    LED_OFF,
					    100);
		else
			led_set_color_power(LED_OFF, 0);
	}
}

/* Called by hook task every TICK(IT8xxx2 500ms) */
static void led_hook_tick_called(void)
{
	led_set_battery();
	led_set_power();
}
DECLARE_HOOK(HOOK_TICK, led_hook_tick_called, HOOK_PRIO_DEFAULT);
