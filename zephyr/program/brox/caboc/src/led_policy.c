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

#define LED_PWM_PERIOD_NS BOARD_LED_HZ_TO_PERIOD_NS(324)

const enum ec_led_id supported_led_ids[] = { EC_LED_ID_BATTERY_LED,
					     EC_LED_ID_POWER_LED };

const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

#define LED_BATTERY EC_LED_ID_BATTERY_LED
#define LED_POWER EC_LED_ID_POWER_LED

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

	/* percent should be clamped between 0% and 100% */
	percent = CLAMP(percent, 0, 100);

	pulse_ns = DIV_ROUND_NEAREST(LED_PWM_PERIOD_NS * percent, 100);

	LOG_DBG("PWM LED %s set percent (%d), pulse %d", ch->dev->name, percent,
		pulse_ns);

	rv = pwm_set(ch->dev, ch->channel, LED_PWM_PERIOD_NS, pulse_ns,
		     ch->flags);
	if (rv) {
		LOG_ERR("pwm_set() failed %s (%d)", ch->dev->name, rv);
	}
}

static void led_set_color_duty(enum ec_led_id led_id, enum led_color color,
			       int duty)
{
	switch (led_id) {
	case LED_BATTERY:
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
		break;
	case LED_POWER:
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
			led_set_color_duty(LED_BATTERY, LED_WHITE,
					   brightness[EC_LED_COLOR_WHITE]);
		else if (brightness[EC_LED_COLOR_AMBER] != 0)
			led_set_color_duty(LED_BATTERY, LED_AMBER,
					   brightness[EC_LED_COLOR_AMBER]);
		else
			led_set_color_duty(LED_BATTERY, LED_OFF, 0);
		break;
	case EC_LED_ID_POWER_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			led_set_color_duty(LED_POWER, LED_WHITE,
					   brightness[EC_LED_COLOR_WHITE]);
		else
			led_set_color_duty(LED_POWER, LED_OFF, 0);
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	return EC_SUCCESS;
}

static struct pwm_led_tick_config {
	enum led_color color;
	uint32_t on_step_time;
	uint32_t off_step_time;
	uint32_t full_on_period;
	uint32_t full_off_period;
	int duty_step_inc;
	int duty_step_dec;
	bool duty_step_is_inc;
	int duty_val;
} batt_led_tick_config, pwr_led_tick_config;

#define PWM_LED_CONFIG_TICK(led_tick_config_ref, color, on_step_time,       \
			    off_step_time, full_on_period, full_off_period, \
			    pulse_on_period, pulse_off_period)              \
	pwm_led_config_tick(                                                \
		(led_tick_config_ref), (color), (on_step_time),             \
		(off_step_time), (full_on_period), (full_off_period),       \
		(pulse_on_period && on_step_time) ?                         \
			(100 / (pulse_on_period / (on_step_time))) :        \
			100,                                                \
		(pulse_off_period && off_step_time) ?                       \
			(100 / (pulse_off_period / (off_step_time))) :      \
			100)

static void pwm_led_config_tick(struct pwm_led_tick_config *led_tick_config,
				enum led_color color, uint32_t on_step_time,
				uint32_t off_step_time, uint32_t full_on_period,
				uint32_t full_off_period, int duty_step_inc,
				int duty_step_dec)
{
	led_tick_config->color = color;
	led_tick_config->on_step_time = on_step_time;
	led_tick_config->off_step_time = off_step_time;
	led_tick_config->full_on_period = full_on_period;
	led_tick_config->full_off_period = full_off_period;
	led_tick_config->duty_step_inc = duty_step_inc;
	led_tick_config->duty_step_dec = duty_step_dec;
	led_tick_config->duty_step_is_inc = true;
	led_tick_config->duty_val = 0;
}

static uint32_t pwm_led_tick_control(enum ec_led_id led_id)
{
	uint32_t next_defer = 0;
	struct pwm_led_tick_config *led_tick_config;

	switch (led_id) {
	case LED_BATTERY:
		led_tick_config = &batt_led_tick_config;
		break;
	case LED_POWER:
		led_tick_config = &pwr_led_tick_config;
		break;
	default:
		return EC_ERROR_PARAM1;
	}

	if (led_auto_control_is_enabled(led_id))
		led_set_color_duty(led_id, led_tick_config->color,
				   led_tick_config->duty_val);

	if (!led_tick_config->duty_step_is_inc &&
	    (led_tick_config->duty_val - led_tick_config->duty_step_dec < 0)) {
		next_defer = led_tick_config->full_off_period;
		led_tick_config->duty_step_is_inc = true;
	} else if (led_tick_config->duty_step_is_inc &&
		   (led_tick_config->duty_val + led_tick_config->duty_step_inc >
		    100)) {
		next_defer = led_tick_config->full_on_period;
		led_tick_config->duty_step_is_inc = false;
	}

	if (led_tick_config->duty_step_is_inc) {
		led_tick_config->duty_val += led_tick_config->duty_step_inc;
		if (next_defer == 0)
			next_defer = led_tick_config->on_step_time;
	} else {
		led_tick_config->duty_val -= led_tick_config->duty_step_dec;
		if (next_defer == 0)
			next_defer = led_tick_config->off_step_time;
	}

	return next_defer;
}

enum batt_led_mode {
	BATT_MODE_NO_CHANGE = 0,
	BATT_MODE_CHG,
	BATT_MODE_CHG_NEAR_FULL,
	BATT_MODE_CHG_IDLE,
	BATT_MODE_CHG_FORCE_IDLE,
	BATT_MODE_DISCHG_NORMAL,
	BATT_MODE_DISCHG_LOW,
	BATT_MODE_DISCHG_CRITICAL,
	BATT_MODE_ERROR,
};
static enum batt_led_mode batt_current_mode;

static void batt_led_tick_control(void);
DECLARE_DEFERRED(batt_led_tick_control);

static void batt_led_tick_control(void)
{
	uint32_t elapsed;
	uint32_t defer = 0;
	uint32_t start = get_time().le.lo;

	defer = pwm_led_tick_control(LED_BATTERY);
	elapsed = get_time().le.lo - start;
	defer = (defer > elapsed) ? (defer - elapsed) : 0;
	hook_call_deferred(&batt_led_tick_control_data, defer);
}

#define BATT_LOW_LED_OFF_STEP_TIME_MS (35 * USEC_PER_MSEC)
#define BATT_LOW_LED_FULL_ON_PERIOD_MS (125 * USEC_PER_MSEC)
#define BATT_LOW_LED_PULSE_OFF_PERIOD_MS (875 * USEC_PER_MSEC)
#define BATT_CRI_LED_OFF_STEP_TIME_MS (75 * USEC_PER_MSEC)
#define BATT_CRI_LED_FULL_ON_PERIOD_MS (125 * USEC_PER_MSEC)
#define BATT_CRI_LED_PULSE_OFF_PERIOD_MS (375 * USEC_PER_MSEC)
#define BATT_ERR_LED_FULL_ON_PERIOD_MS (500 * USEC_PER_MSEC)
#define BATT_ERR_LED_FULL_OFF_PERIOD_MS (500 * USEC_PER_MSEC)
#define BATT_FORCE_IDLE_LED_FULL_ON_PERIOD_MS (1000 * USEC_PER_MSEC)
#define BATT_FORCE_IDLE_LED_FULL_OFF_PERIOD_MS (1000 * USEC_PER_MSEC)

static void battery_led_mode_set(enum batt_led_mode new_mode)
{
	if (new_mode != BATT_MODE_NO_CHANGE) {
		batt_current_mode = new_mode;
		hook_call_deferred(&batt_led_tick_control_data, -1);
	}

	if (led_auto_control_is_enabled(LED_BATTERY)) {
		switch (new_mode) {
		case BATT_MODE_CHG:
			led_set_color_duty(LED_BATTERY, LED_AMBER, 100);
			break;
		case BATT_MODE_CHG_NEAR_FULL:
		case BATT_MODE_CHG_IDLE:
			led_set_color_duty(LED_BATTERY, LED_WHITE, 100);
			break;
		case BATT_MODE_CHG_FORCE_IDLE:
			PWM_LED_CONFIG_TICK(
				&batt_led_tick_config, LED_AMBER, 0, 0,
				BATT_FORCE_IDLE_LED_FULL_ON_PERIOD_MS,
				BATT_FORCE_IDLE_LED_FULL_OFF_PERIOD_MS, 0, 0);
			hook_call_deferred(&batt_led_tick_control_data, 0);
			break;
		case BATT_MODE_DISCHG_NORMAL:
			led_set_color_duty(LED_BATTERY, LED_OFF, 0);
			break;
		case BATT_MODE_DISCHG_LOW:
			PWM_LED_CONFIG_TICK(&batt_led_tick_config, LED_AMBER, 0,
					    BATT_LOW_LED_OFF_STEP_TIME_MS,
					    BATT_LOW_LED_FULL_ON_PERIOD_MS, 0,
					    0,
					    BATT_LOW_LED_PULSE_OFF_PERIOD_MS);
			hook_call_deferred(&batt_led_tick_control_data, 0);
			break;
		case BATT_MODE_DISCHG_CRITICAL:
			PWM_LED_CONFIG_TICK(&batt_led_tick_config, LED_AMBER, 0,
					    BATT_CRI_LED_OFF_STEP_TIME_MS,
					    BATT_CRI_LED_FULL_ON_PERIOD_MS, 0,
					    0,
					    BATT_CRI_LED_PULSE_OFF_PERIOD_MS);
			hook_call_deferred(&batt_led_tick_control_data, 0);
			break;
		case BATT_MODE_ERROR:
			PWM_LED_CONFIG_TICK(&batt_led_tick_config, LED_AMBER, 0,
					    0, BATT_ERR_LED_FULL_ON_PERIOD_MS,
					    BATT_ERR_LED_FULL_OFF_PERIOD_MS, 0,
					    0);
			hook_call_deferred(&batt_led_tick_control_data, 0);
			break;
		case BATT_MODE_NO_CHANGE:
		default:
			break;
		}
	} else /* LED_BATTERY auto control disabled */
		hook_call_deferred(&batt_led_tick_control_data, -1);
}

static void battery_led_mode_check(void)
{
	enum batt_led_mode new_mode;

	switch (led_pwr_get_state()) {
	case LED_PWRS_CHARGE:
		new_mode = BATT_MODE_CHG;
		break;
	case LED_PWRS_DISCHARGE:
		if (charge_get_percent() <= BATTERY_LEVEL_CRITICAL)
			new_mode = BATT_MODE_DISCHG_CRITICAL;
		else if (charge_get_percent() < BATT_LOW_BCT &&
			 charge_get_percent() > BATTERY_LEVEL_CRITICAL)
			new_mode = BATT_MODE_DISCHG_LOW;
		else
			new_mode = BATT_MODE_DISCHG_NORMAL;
		break;
	case LED_PWRS_ERROR:
		new_mode = BATT_MODE_ERROR;
		break;
	case LED_PWRS_CHARGE_NEAR_FULL:
		new_mode = BATT_MODE_CHG_NEAR_FULL;
		break;
	case LED_PWRS_IDLE: /* External power connected in IDLE */
		new_mode = BATT_MODE_CHG_IDLE;
		break;
	case LED_PWRS_FORCED_IDLE:
		new_mode = BATT_MODE_CHG_FORCE_IDLE;
		break;
	default:
		/* Other states don't alter LED behavior */
		new_mode = batt_current_mode;
		break;
	}

	if (new_mode == batt_current_mode)
		new_mode = BATT_MODE_NO_CHANGE;

	battery_led_mode_set(new_mode);
}
DECLARE_HOOK(HOOK_TICK, battery_led_mode_check, HOOK_PRIO_DEFAULT);

enum power_led_mode {
	PWR_MODE_NO_CHANGE = 0,
	PWR_MODE_ON,
	PWR_MODE_SUSPEND,
	PWR_MODE_OFF,
};
enum power_led_mode pwr_current_mode;

static void pwr_led_tick_control(void);
DECLARE_DEFERRED(pwr_led_tick_control);

static void pwr_led_tick_control(void)
{
	uint32_t elapsed;
	uint32_t defer = 0;
	uint32_t start = get_time().le.lo;

	defer = pwm_led_tick_control(LED_POWER);
	elapsed = get_time().le.lo - start;
	defer = (defer > elapsed) ? (defer - elapsed) : 0;
	hook_call_deferred(&pwr_led_tick_control_data, defer);
}

/*
 * Due to the CSME-Lite processing, upon startup the CPU transitions through
 * S0->S3->S5->S3->S0, causing the LED to turn on/off/on, so
 * delay turning off power LED during suspend/shutdown.
 */
#define PWR_LED_CPU_DELAY K_MSEC(2000)

#define PWR_SUSPEND_LED_FULL_ON_PERIOD_MS (1000 * USEC_PER_MSEC)
#define PWR_SUSPEND_LED_FULL_OFF_PERIOD_MS (1000 * USEC_PER_MSEC)

static void power_led_mode_set(enum power_led_mode new_mode)
{
	if (new_mode != PWR_MODE_NO_CHANGE) {
		pwr_current_mode = new_mode;
		hook_call_deferred(&pwr_led_tick_control_data, -1);
	}

	if (led_auto_control_is_enabled(LED_POWER)) {
		switch (new_mode) {
		case PWR_MODE_ON:
			led_set_color_duty(LED_POWER, LED_WHITE, 100);
			break;
		case PWR_MODE_SUSPEND:
			PWM_LED_CONFIG_TICK(&pwr_led_tick_config, LED_WHITE, 0,
					    0,
					    PWR_SUSPEND_LED_FULL_ON_PERIOD_MS,
					    PWR_SUSPEND_LED_FULL_OFF_PERIOD_MS,
					    0, 0);
			hook_call_deferred(&pwr_led_tick_control_data, 0);
			break;
		case PWR_MODE_OFF:
			led_set_color_duty(LED_POWER, LED_OFF, 0);
			break;
		case PWR_MODE_NO_CHANGE:
		default:
			break;
		}
	} else /* LED_BATTERY auto control disabled */
		hook_call_deferred(&pwr_led_tick_control_data, -1);
}

/*
 * Timer for handling delays on suspend and shutdown. This needs
 * to be cancellable from non-workqueue threads, so it uses a timer
 * rather than deferred work because deferred work may be impossible
 * to cancel if currently running because it was preempted.
 */
K_TIMER_DEFINE(shutdown_timer, NULL, NULL);

static void pwr_led_suspend(struct k_timer *unused_timer)
{
	power_led_mode_set(PWR_MODE_SUSPEND);
}

static void pwr_led_shutdown(struct k_timer *unused_timer)
{
	power_led_mode_set(PWR_MODE_OFF);
}

static void pwr_led_shutdown_hook(void)
{
	k_timer_stop(&shutdown_timer);
	k_timer_init(&shutdown_timer, pwr_led_shutdown, NULL);
	k_timer_start(&shutdown_timer, PWR_LED_CPU_DELAY, K_FOREVER);
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, pwr_led_shutdown_hook, HOOK_PRIO_DEFAULT);

static void pwr_led_suspend_hook(void)
{
	k_timer_stop(&shutdown_timer);
	k_timer_init(&shutdown_timer, pwr_led_suspend, NULL);
	k_timer_start(&shutdown_timer, PWR_LED_CPU_DELAY, K_FOREVER);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, pwr_led_suspend_hook, HOOK_PRIO_DEFAULT);

static void pwr_led_resume(void)
{
	/*
	 * Avoid invoking the suspend/shutdown delayed hooks.
	 */
	k_timer_stop(&shutdown_timer);

	power_led_mode_set(PWR_MODE_ON);
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, pwr_led_resume, HOOK_PRIO_DEFAULT);

/*
 * Since power led is controlled by functions called only when power state
 * change, we need to make sure that power led is in right state when EC
 * init, especially for sysjump case.
 */
static void pwr_led_init(void)
{
	if (chipset_in_state(CHIPSET_STATE_ON))
		pwr_led_resume();
	else if (chipset_in_state(CHIPSET_STATE_SUSPEND))
		pwr_led_suspend_hook();
	else
		pwr_led_shutdown_hook();
}
DECLARE_HOOK(HOOK_INIT, pwr_led_init, HOOK_PRIO_DEFAULT);

void board_led_auto_control(void)
{
	battery_led_mode_set(batt_current_mode);
	power_led_mode_set(pwr_current_mode);
}
