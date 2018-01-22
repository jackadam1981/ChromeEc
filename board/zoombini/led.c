/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Zoombini/Meowth specific LED settings. */

#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "pwm.h"
#include "rgb_led.h"
#include "util.h"

const enum ec_led_id supported_led_ids[] = {
#ifdef BOARD_MEOWTH
	EC_LED_ID_LEFT_LED,
	EC_LED_ID_RIGHT_LED,
#else
	EC_LED_ID_POWER_LED,
#endif /* defined(BOARD_MEOWTH) */
};
const int supported_led_ids_count = ARRAY_SIZE(supported_led_ids);

/* We won't be using the blue channel long term. */
struct rgb led_color_map[EC_LED_COLOR_COUNT] = {
	[EC_LED_COLOR_RED]    = {   8,   0,   0 },
	[EC_LED_COLOR_GREEN]  = {   0,   8,   0 },
	[EC_LED_COLOR_BLUE]   = {   0,   0,   0 },
	[EC_LED_COLOR_YELLOW] = {   8,  16,   0 },
	[EC_LED_COLOR_WHITE]  = {   0,   0,   0 },
	[EC_LED_COLOR_AMBER]  = {  12,   9,   0 },
};

#ifdef BOARD_MEOWTH
/* Meowth LED definitions */
struct rgb rgb_leds[CONFIG_RGB_LED_COUNT] = {
	{
		.red = PWM_CH_DB0_LED_RED,
		.green = PWM_CH_DB0_LED_GREEN,
		.blue = PWM_CH_DB0_LED_BLUE,
	},

	{
		.red = PWM_CH_DB1_LED_RED,
		.green = PWM_CH_DB1_LED_GREEN,
		.blue = PWM_CH_DB1_LED_BLUE,
	},
};
#else
/* Zoombini LED definitions. */
struct rgb rgb_leds[CONFIG_RGB_LED_COUNT] = {
	{
		.red = PWM_CH_LED_RED,
		.green = PWM_CH_LED_GREEN,
		.blue = RGB_LED_NO_CHANNEL,
	},
};
#endif /* !defined(BOARD_MEOWTH) */

void led_get_brightness_range(enum ec_led_id led_id, uint8_t *brightness_range)
{
	brightness_range[EC_LED_COLOR_RED] = 100;
	brightness_range[EC_LED_COLOR_GREEN] = 100;
	brightness_range[EC_LED_COLOR_YELLOW] = 100;
	brightness_range[EC_LED_COLOR_AMBER] = 100;
	/* Zoombini has no blue channel; it's also going away for Meowth. */
	brightness_range[EC_LED_COLOR_BLUE] = 0;
	brightness_range[EC_LED_COLOR_WHITE] = 0;
}

int led_set_brightness(enum ec_led_id led_id, const uint8_t *brightness)
{
	switch (led_id) {
#ifdef BOARD_MEOWTH
	case EC_LED_ID_LEFT_LED:
	case EC_LED_ID_RIGHT_LED:
		led_id -= EC_LED_ID_LEFT_LED; /* convert to rgb id */
#else /* defined(BOARD_ZOOMBINI) */
	case EC_LED_ID_POWER_LED:
		led_id -= EC_LED_ID_POWER_LED;
#endif /* defined(BOARD_MEOWTH) */
		break;

		if (brightness[EC_LED_COLOR_RED])
			set_rgb_led_color(led_id, EC_LED_COLOR_RED);
		else if (brightness[EC_LED_COLOR_GREEN])
			set_rgb_led_color(led_id, EC_LED_COLOR_GREEN);
		else if (brightness[EC_LED_COLOR_YELLOW])
			set_rgb_led_color(led_id, EC_LED_COLOR_YELLOW);
		else if (brightness[EC_LED_COLOR_AMBER])
			set_rgb_led_color(led_id, EC_LED_COLOR_AMBER);
		break;

	default:
		return EC_ERROR_UNKNOWN;
	}
	return EC_SUCCESS;
}
