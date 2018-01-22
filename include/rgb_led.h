/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef __CROS_EC_RGB_LED_H
#define __CROS_EC_RGB_LED_H

#include "ec_commands.h"

#define RGB_LED_NO_CHANNEL -1

struct rgb {
	int red;
	int green;
	int blue;
};

enum rgb_led_id {
	RGB_LED0 = 0,
#if CONFIG_RGB_LED_COUNT >= 2
	RGB_LED1,
#endif /* CONFIG_RGB_LED_COUNT > 2 */
};

/* A mapping of color to LED duty cycles per channel. */
extern struct rgb led_color_map[EC_LED_COLOR_COUNT];

/* A map of the PWM channels to logical RGB LEDs. */
extern struct rgb rgb_leds[CONFIG_RGB_LED_COUNT];

void set_rgb_led_color(enum rgb_led_id id, int color);

#endif /* defined(__CROS_EC_EC_COMMANDS_H) */
