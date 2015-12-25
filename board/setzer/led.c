/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Power/Battery LED control for Setzer
 */

#include "charge_state.h"
#include "chipset.h"
#include "console.h"
#include "extpower.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "registers.h"
#include "util.h"

#define CPRINTF(format, args...) cprintf(CC_PWM, format, ## args)
#define CPRINTS(format, args...) cprints(CC_PWM, format, ## args)

#define LED_TOTAL_TICKS 16
#define LED_ON_TICKS 4

static int led_debug;

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_POWER_LED, EC_LED_ID_BATTERY_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_WHITE,
	LED_AMBER,
	LED_COLOR_COUNT  /* Number of colors, not a color itself */
};

static int bat_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_BAT_LED0, 0);
		pwm_set_duty(PWM_CH_BAT_LED1, 0);
		break;
	case LED_WHITE:
		pwm_set_duty(PWM_CH_BAT_LED0, 100);
		pwm_set_duty(PWM_CH_BAT_LED1, 0);
		break;
	case LED_AMBER:
		pwm_set_duty(PWM_CH_BAT_LED0, 0);
		pwm_set_duty(PWM_CH_BAT_LED1, 100);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

static int pwr_led_set_color(enum led_color color)
{
	switch (color) {
	case LED_OFF:
		pwm_set_duty(PWM_CH_PWR_LED, 0);
		break;
	case LED_WHITE:
		pwm_set_duty(PWM_CH_PWR_LED, 100);
		break;
	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 100;
		brightness_range[EC_LED_COLOR_YELLOW] = 100;
		break;
	case EC_LED_ID_POWER_LED:
		brightness_range[EC_LED_COLOR_WHITE] = 100;
		break;
	default:
		/* Nothing to do */
		break;
	}
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
	case EC_LED_ID_BATTERY_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			bat_led_set_color(LED_WHITE);
		else if (brightness[EC_LED_COLOR_YELLOW] != 0)
			bat_led_set_color(LED_AMBER);
		else
			bat_led_set_color(LED_OFF);
		break;
	case EC_LED_ID_POWER_LED:
		if (brightness[EC_LED_COLOR_WHITE] != 0)
			pwr_led_set_color(LED_WHITE);
		else
			pwr_led_set_color(LED_OFF);
		break;
	default:
		break;
	}
	return EC_SUCCESS;
}

static void setzer_led_set_power(void)
{
	static int power_ticks;
	static int previous_state_suspend;

	power_ticks++;

	if (chipset_in_state(CHIPSET_STATE_SUSPEND)) {
		/* Reset ticks if entering suspend so LED turns amber
		 * as soon as possible. */
		if (!previous_state_suspend)
			power_ticks = 0;

		/* Blink once every one second. */
		pwr_led_set_color((power_ticks & 0x4) ? LED_WHITE : LED_OFF);
		previous_state_suspend = 1;
		return;
	}

	previous_state_suspend = 0;

	if (chipset_in_state(CHIPSET_STATE_ANY_OFF))
		pwr_led_set_color(LED_OFF);
	else if (chipset_in_state(CHIPSET_STATE_ON))
		pwr_led_set_color(LED_WHITE);
}

static void setzer_led_set_battery(void)
{
	static int battery_ticks;

	battery_ticks++;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		bat_led_set_color(LED_AMBER);
		break;
	case PWR_STATE_CHARGE_NEAR_FULL:
		bat_led_set_color(LED_WHITE);
		break;
	case PWR_STATE_DISCHARGE:
		/* See crosbug.com/p/22159. There's a 3% difference
		 * between the battery level seen by the kernel and what's
		 * really going on, so if they want to see 12%, we use 10%.
		 * Hard code this number here, because this only affects the
		 * LED color, not the battery charge state. */
		if (charge_get_percent() < 10)
			bat_led_set_color(
			(battery_ticks & 0x4) ? LED_WHITE : LED_OFF);
		else
			bat_led_set_color(LED_OFF);
		break;
	case PWR_STATE_ERROR:
		bat_led_set_color((battery_ticks & 0x2) ? LED_WHITE : LED_OFF);
		break;
	case PWR_STATE_IDLE:
		bat_led_set_color(LED_WHITE);
		break;
	default:
		/* Other states don't alter LED behavior */
		break;
	}
}

static void led_init(void)
{
	/* Configure GPIOs */
	gpio_config_module(MODULE_PWM_LED, 1);

	/*
	 * Enable PWMs and set to 0% duty cycle.  If they're disabled,
	 * seems to ground the pins instead of letting them float.
	 */
	pwm_enable(PWM_CH_BAT_LED0, 1);
	pwm_enable(PWM_CH_BAT_LED1, 1);
	pwm_enable(PWM_CH_PWR_LED, 1);

	bat_led_set_color(LED_OFF);
	pwr_led_set_color(LED_OFF);
}
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_DEFAULT);

/**
 * Called by hook task every 250 ms
 */
static void led_tick(void)
{
	if (led_debug)
		return;

	if (extpower_is_present()) {
		if (led_auto_control_is_enabled(EC_LED_ID_BATTERY_LED)) {
			setzer_led_set_battery();
			return;
		}
	} else if (led_auto_control_is_enabled(EC_LED_ID_POWER_LED)) {
		setzer_led_set_power();
		return;
	}

	bat_led_set_color(LED_OFF);
	pwr_led_set_color(LED_OFF);
}
DECLARE_HOOK(HOOK_TICK, led_tick, HOOK_PRIO_DEFAULT);

static void dump_pwm_channels(void)
{
	int ch;
	for (ch = 0; ch < 4; ch++) {
		CPRINTF("channel = %d\n", ch);
		CPRINTF("0x%04X 0x%04X 0x%04X\n",
			MEC1322_PWM_CFG(ch),
			MEC1322_PWM_ON(ch),
			MEC1322_PWM_OFF(ch));
	}
}
/******************************************************************/
/* Console commands */
static int command_led_color(int argc, char **argv)
{
	if (argc > 1) {
		if (!strcasecmp(argv[1], "debug")) {
			led_debug ^= 1;
			CPRINTF("led_debug = %d\n", led_debug);
		} else if (!strcasecmp(argv[1], "off")) {
			bat_led_set_color(LED_OFF);
			pwr_led_set_color(LED_OFF);
		} else if (!strcasecmp(argv[1], "amber")) {
			bat_led_set_color(LED_AMBER);
		} else {
			/* maybe handle charger_discharge_on_ac() too? */
			return EC_ERROR_PARAM1;
		}
	}

	if (led_debug == 1)
		dump_pwm_channels();
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(ledcolor, command_led_color,
			"[debug|red|green|amber|off]",
			"Change LED color",
			NULL);

