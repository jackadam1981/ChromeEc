/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Battery LED control for Clapper
 */

#include "charge_state.h"
#include "chipset.h"
#include "gpio.h"
#include "hooks.h"
#include "led_common.h"
#include "pwm.h"
#include "util.h"

#define POWER_LED_ON gpio_set_level(GPIO_POWER_LED_L, 0)
#define POWER_LED_OFF gpio_set_level(GPIO_POWER_LED_L, 1)

const enum ec_led_id supported_led_ids[] = {
	EC_LED_ID_BATTERY_LED, EC_LED_ID_POWER_LED};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

enum led_color {
	LED_OFF = 0,
	LED_AMBER,
	LED_WHITE,

	/* Number of colors, not a color itself */
	LED_COLOR_COUNT
};

/* Brightness vs. color, for {red, green} LEDs */
static const uint8_t color_brightness[LED_COLOR_COUNT][2] = {
	{0, 0},
	{100, 0},
	{0, 100},
};

static void led_update(void);
static void power_led_update(void);
static void battery_led_update(void);

/**
 * Set LED color
 *
 * @param color		Enumerated color value
 */
static void set_color(enum led_color color)
{
	pwm_set_duty(PWM_CH_LED_AMBER, color_brightness[color][0]);
	pwm_set_duty(PWM_CH_LED_WHITE, color_brightness[color][1]);
}

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_RED] = 100;
	brightness_range[EC_LED_COLOR_WHITE] = 100;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	pwm_set_duty(PWM_CH_LED_AMBER, brightness[LED_AMBER]);
	pwm_set_duty(PWM_CH_LED_WHITE, brightness[LED_WHITE]);
	return EC_SUCCESS;
}

static void led_init(void)
{
	/* Configure GPIOs */
	gpio_config_module(MODULE_PWM_LED, 1);

	/*
	 * Enable PWMs and set to 0% duty cycle.  If they're disabled, the LM4
	 * seems to ground the pins instead of letting them float.
	 */
	pwm_enable(PWM_CH_LED_AMBER, 1);
	pwm_enable(PWM_CH_LED_WHITE, 1);
	set_color(LED_OFF);
	hook_call_deferred(led_update, 0);
}
DECLARE_HOOK(HOOK_INIT, led_init, HOOK_PRIO_DEFAULT);

/**
 * Called every 100 ms
 */
static void led_update(void)
{
	int delay = 100 * MSEC;

	power_led_update();
	battery_led_update();
	hook_call_deferred(led_update, delay);
}
DECLARE_DEFERRED(led_update);

static void power_led_update(void)
{
	static unsigned ticks;

	/* 2 sec */
	if (++ticks > 20)
		ticks = 0;
	/* S3 */
	if (chipset_in_state(CHIPSET_STATE_SUSPEND)) {
		/* 1 sec on */
		if (ticks <= 10)
			POWER_LED_ON;
		/* 1 sec off */
		else
			POWER_LED_OFF;
	}
	/* S0 */
	else if (chipset_in_state(CHIPSET_STATE_ON))
		POWER_LED_ON;
	/* S5 */
	else
		POWER_LED_OFF;
}

static void battery_led_update(void)
{
	static unsigned timer;
	unsigned battery = charge_get_percent();

	/* least common multiple : 15, 33, 51 */
	if (++timer == 2805)
		timer = 0;

	switch (charge_get_state()) {
	case PWR_STATE_CHARGE:
		/* 1%~4% */
		if (battery > 0 && battery < 5) {
			/* 1000 ms on */
			if (timer%15 < 10)
				set_color(LED_AMBER);
			/* 500 ms off */
			else
				set_color(LED_OFF);
		}
		/* 5% ~ 19% */
		else if (battery >= 5 && battery < 20) {
			/* 3200 ms on */
			if (timer%33 < 32)
				set_color(LED_AMBER);
			/* 100 ms off */
			else
				set_color(LED_OFF);
		}
		/* 20% ~ 79% */
		else if (battery >= 20 && battery < 80) {
			/* 5000 ms on */
			if (timer%51 < 50)
				set_color(LED_WHITE);
			/* 100 ms off */
			else
				set_color(LED_OFF);
		}
		/* 80%~100% */
		else if (battery >= 80 && battery <= 100)
			set_color(LED_WHITE);
		break;
	case PWR_STATE_DISCHARGE:	/* without AC */
	case PWR_STATE_UNCHANGE:	/* with AC */
	case PWR_STATE_CHARGE_NEAR_FULL:
	case PWR_STATE_IDLE0:
	case PWR_STATE_IDLE:
		/* not S0 */
		if (!chipset_in_state(CHIPSET_STATE_ON))
			set_color(LED_OFF);
		/* 1%~4% */
		else if (battery > 0 && battery < 5) {
			/* 1000 ms on */
			if (timer%15 < 10)
				set_color(LED_AMBER);
			/* 500 ms off */
			else
				set_color(LED_OFF);
		}
		/* 5% ~ 19% */
		else if (battery >= 5 && battery < 20)
			set_color(LED_AMBER);
		/* 20%~100% */
		else if (battery >= 20 && battery <= 100)
			set_color(LED_WHITE);
		break;
	case PWR_STATE_INIT:
	case PWR_STATE_REINIT:
	case PWR_STATE_ERROR:
		break;
	}
}
